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
    return (qtype == DNS_TYPE_A || qtype == DNS_TYPE_AAAA);
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

    if (hostdb_init(&local_record, NULL, cfg.interface_name) != 0) {
        log_error("Failed to initialize host database");
        log_close();
        return 1;
    }

    sockfd = mdns_socket_open(cfg.interface_name);
    if (sockfd < 0) {
        log_error("Failed to open mDNS socket on interface %s", cfg.interface_name);
        log_close();
        return 1;
    }

    {
        struct sigaction sa;
        sa.sa_handler = on_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
    }

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

            if (hostdb_lookup(&local_record, question.name, &match) != 1) {
                log_debug("No match for qname %s", question.name);
                continue;
            }

            {
                int out_len;
                ssize_t sent;
                struct sockaddr_in6 mdns_dst;

                out_len = mdns_build_response(out_buf, sizeof(out_buf), &question, &match);
                if (out_len <= 0) {
                    continue;
                }

                memset(&mdns_dst, 0, sizeof(mdns_dst));
                mdns_dst.sin6_family = AF_INET6;
                mdns_dst.sin6_port = htons(MDNS_PORT);
                if (inet_pton(AF_INET6, "ff02::fb", &mdns_dst.sin6_addr) != 1) {
                    log_warn("Failed to set mDNS multicast destination");
                    continue;
                }

                sent = sendto(sockfd, out_buf, (size_t)out_len, 0,
                              (struct sockaddr *)&mdns_dst, sizeof(mdns_dst));
                if (sent < 0) {
                    log_warn("sendto failed: %s", strerror(errno));
                } else {
                    log_info("Answered %s type %u", question.name, question.qtype);
                }
            }
        }
    }

    log_info("mdnsd shutting down");
    mdns_socket_close(sockfd);
    log_close();
    return 0;
}
