#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "args.h"
#include "config.h"
#include "hostdb.h"
#include "log.h"
#include "mdns.h"
#include "socket.h"

static volatile sig_atomic_t g_running = 1;

static void on_signal(int signo) {
    (void)signo;
    g_running = 0;
}

static int is_supported_query_type(uint16_t qtype) {
    return (qtype == DNS_TYPE_A || qtype == DNS_TYPE_AAAA || qtype == DNS_TYPE_SRV);
}

// Parse service type from query name.
// Returns 0 if it looks like a general service query (_service._proto.domain)
// Returns 1 if it looks like a targeted instance query
static int is_general_service_query(const char *qname) {
    // General service queries start with underscore
    return (qname[0] == '_');
}

// Extract service type and domain from general query name
// E.g., "_http._tcp.local" -> service_type="_http._tcp", domain="local"
static int parse_service_type_query(const char *qname, char *service_type,
                                     size_t st_len, char *domain, size_t dom_len) {
    const char *second_dot;
    const char *third_dot;
    
    if (qname[0] != '_') {
        return -1;
    }
    
    // Find second underscore/dot
    second_dot = strchr(qname + 1, '.');
    if (second_dot == NULL) {
        return -1;
    }
    
    // Check if next part starts with underscore
    if (second_dot[1] != '_') {
        return -1;
    }
    
    // Find the dot after _tcp or _udp
    third_dot = strchr(second_dot + 1, '.');
    if (third_dot == NULL) {
        return -1;
    }
    
    // Extract service type
    size_t st_size = (size_t)(third_dot - qname);
    if (st_size >= st_len) {
        return -1;
    }
    memcpy(service_type, qname, st_size);
    service_type[st_size] = '\0';
    
    // Extract domain (rest after third dot)
    size_t dom_size = strlen(third_dot + 1);
    if (dom_size >= dom_len) {
        return -1;
    }
    strcpy(domain, third_dot + 1);
    
    // Remove trailing dot if present
    if (dom_size > 0 && domain[dom_size - 1] == '.') {
        domain[dom_size - 1] = '\0';
    }
    
    return 0;
}

int main(int argc, char **argv) {
    app_config_t cfg;
    host_record_t local_record;
    int sockfd;

    if (parse_args(argc, argv, &cfg) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    if (log_init(cfg.verbosity, cfg.log_target == LOG_TARGET_SYSLOG) != 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        return 1;
    }

    if (hostdb_init(&local_record, NULL) != 0) {
        log_error("Failed to initialize host database");
        log_close();
        return 1;
    }

    if (cfg.config_path != NULL) {
        int loaded = config_load_services(cfg.config_path);
        if (loaded < 0) {
            log_warn("Could not load config file, continuing without services");
        }
    }

    sockfd = mdns_socket_open(cfg.interface_name);
    if (sockfd < 0) {
        log_error("Failed to open mDNS socket on interface %s", cfg.interface_name);
        log_close();
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    log_info("mdnsd started on interface %s for host %s", cfg.interface_name, local_record.hostname);

    while (g_running) {
        fd_set rfds;
        struct timeval tv;
        int ready;

        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        ready = select(sockfd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error("select failed: %s", strerror(errno));
            break;
        }
        if (ready == 0) {
            continue;
        }

        if (FD_ISSET(sockfd, &rfds)) {
            uint8_t in_buf[MDNS_MAX_PACKET];
            uint8_t out_buf[MDNS_MAX_PACKET];
            struct sockaddr_in6 src_addr;
            socklen_t src_len = sizeof(src_addr);
            ssize_t nread;
            dns_question_t question;
            int parsed;
            host_record_t match;

            nread = recvfrom(sockfd, in_buf, sizeof(in_buf), 0, (struct sockaddr *)&src_addr, &src_len);
            if (nread < 0) {
                log_warn("recvfrom failed: %s", strerror(errno));
                continue;
            }

            parsed = mdns_parse_query(in_buf, (size_t)nread, &question);
            if (parsed <= 0) {
                continue;
            }

            if (!is_supported_query_type(question.qtype)) {
                log_debug("Ignoring unsupported qtype %u for %s", question.qtype, question.name);
                continue;
            }

            // Handle A/AAAA queries
            if (question.qtype == DNS_TYPE_A || question.qtype == DNS_TYPE_AAAA) {
                if (hostdb_lookup(&local_record, question.name, &match) != 1) {
                    log_debug("No match for qname %s", question.name);
                    continue;
                }

                int out_len;
                ssize_t sent;

                out_len = mdns_build_response(out_buf, sizeof(out_buf), &question, &match);
                if (out_len <= 0) {
                    continue;
                }

                sent = sendto(sockfd, out_buf, (size_t)out_len, 0, (struct sockaddr *)&src_addr, src_len);
                if (sent < 0) {
                    log_warn("sendto failed: %s", strerror(errno));
                } else {
                    log_info("Answered %s type %u", question.name, question.qtype);
                }
            }
            // Handle SRV queries
            else if (question.qtype == DNS_TYPE_SRV) {
                mdns_service_t *services[32];
                size_t service_count = 0;
                int out_len;
                ssize_t sent;
                
                if (is_general_service_query(question.name)) {
                    // General query: return all services of this type
                    char service_type[256];
                    char domain[256];
                    
                    if (parse_service_type_query(question.name, service_type,
                                                 sizeof(service_type), domain,
                                                 sizeof(domain)) == 0) {
                        service_count = mdns_find_services_by_type(service_type, domain,
                                                                   services, 32);
                    }
                } else {
                    // Targeted query: return specific instance
                    mdns_service_t *svc = mdns_find_service_by_fqdn(question.name);
                    if (svc != NULL) {
                        services[0] = svc;
                        service_count = 1;
                    }
                }
                
                if (service_count == 0) {
                    log_debug("No service match for %s", question.name);
                    continue;
                }
                
                out_len = mdns_build_service_response(out_buf, sizeof(out_buf),
                                                      &question, services, service_count);
                if (out_len <= 0) {
                    continue;
                }
                
                sent = sendto(sockfd, out_buf, (size_t)out_len, 0,
                             (struct sockaddr *)&src_addr, src_len);
                if (sent < 0) {
                    log_warn("sendto failed: %s", strerror(errno));
                } else {
                    log_info("Answered %s SRV with %zu service(s)", question.name, service_count);
                }
            }
        }
    }

    log_info("mdnsd shutting down");
    mdns_socket_close(sockfd);
    mdns_cleanup_services();
    log_close();
    return 0;
}
