#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "log.h"
#include "mdns.h"

typedef struct {
    const char *service_type;
    int timeout_seconds;
    const char *interface_name;
    int verbose;
    log_level_t verbosity;
} browse_config_t;

static uint16_t read_u16(const uint8_t *ptr) {
    return (uint16_t)((ptr[0] << 8) | ptr[1]);
}

static uint32_t read_u32(const uint8_t *ptr) {
    return ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) | ((uint32_t)ptr[2] << 8) | (uint32_t)ptr[3];
}

static long now_ms(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return 0;
    }
    return (long)(tv.tv_sec * 1000L) + (long)(tv.tv_usec / 1000L);
}

static void print_usage(const char *progname) {
    fprintf(stderr,
            "mDNS Browser - Browse service instances by type\n\n"
            "Usage: %s -s <service-type> [-w <seconds>] [-i <interface>] [-v]\n\n"
            "Options:\n"
            "  -s, --service   Service type to browse (e.g. _http._tcp.local) [required]\n"
            "  -w, --timeout   Seconds to wait for responses (default: 2)\n"
            "  -i, --interface Network interface name (optional, e.g. eth0)\n"
            "  -v, --verbose   Verbose output\n"
            "  -h, --help      Show this help\n",
            progname);
}

static int parse_args(int argc, char **argv, browse_config_t *cfg) {
    static struct option long_opts[] = {
        {"service", required_argument, 0, 's'},
        {"timeout", required_argument, 0, 'w'},
        {"interface", required_argument, 0, 'i'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt;

    if (cfg == NULL) {
        return -1;
    }

    cfg->service_type = NULL;
    cfg->timeout_seconds = 2;
    cfg->interface_name = NULL;
    cfg->verbose = 0;
    cfg->verbosity = APP_LOG_WARN;

    while ((opt = getopt_long(argc, argv, "s:w:i:vh", long_opts, NULL)) != -1) {
        switch (opt) {
            case 's':
                cfg->service_type = optarg;
                break;
            case 'w': {
                char *endptr = NULL;
                long timeout = strtol(optarg, &endptr, 10);
                if (endptr == optarg || *endptr != '\0' || timeout <= 0 || timeout > 3600) {
                    fprintf(stderr, "Invalid timeout: %s\n", optarg);
                    return -1;
                }
                cfg->timeout_seconds = (int)timeout;
                break;
            }
            case 'i':
                cfg->interface_name = optarg;
                break;
            case 'v':
                cfg->verbose = 1;
                cfg->verbosity = APP_LOG_DEBUG;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                return -1;
        }
    }

    if (cfg->service_type == NULL) {
        fprintf(stderr, "Missing required service type\n");
        return -1;
    }

    return 0;
}

static int build_fqdn_service_type(const char *input, char *output, size_t output_len) {
    size_t len;

    if (input == NULL || output == NULL || output_len == 0) {
        return -1;
    }

    len = strlen(input);
    if (len >= output_len) {
        return -1;
    }

    if (len >= 6 && strcasecmp(&input[len - 6], ".local") == 0) {
        memcpy(output, input, len + 1);
        return 0;
    }

    if (snprintf(output, output_len, "%s.local", input) < 0) {
        return -1;
    }

    return 0;
}

static int encode_qname(const char *name, uint8_t *out, size_t out_len, size_t *written_out) {
    const char *cursor = name;
    size_t written = 0;

    while (*cursor != '\0') {
        const char *dot = strchr(cursor, '.');
        size_t label_len = dot ? (size_t)(dot - cursor) : strlen(cursor);

        if (label_len == 0 || label_len > 63) {
            return -1;
        }
        if (written + 1 + label_len >= out_len) {
            return -1;
        }

        out[written++] = (uint8_t)label_len;
        memcpy(&out[written], cursor, label_len);
        written += label_len;

        if (dot == NULL) {
            break;
        }
        cursor = dot + 1;
    }

    if (written >= out_len) {
        return -1;
    }

    out[written++] = 0;
    *written_out = written;
    return 0;
}

static int decode_name(const uint8_t *packet, size_t packet_len, size_t offset,
                       char *out, size_t out_len, size_t *next_offset) {
    size_t pos = offset;
    size_t out_pos = 0;
    size_t jumps = 0;
    int jumped = 0;
    size_t local_next = offset;

    if (packet == NULL || out == NULL || out_len == 0 || next_offset == NULL) {
        return -1;
    }

    while (pos < packet_len) {
        uint8_t len = packet[pos];

        if (len == 0) {
            if (out_pos == 0) {
                if (out_len < 2) {
                    return -1;
                }
                out[0] = '.';
                out[1] = '\0';
            } else {
                out[out_pos - 1] = '\0';
            }

            if (!jumped) {
                local_next = pos + 1;
            }
            *next_offset = local_next;
            return 0;
        }

        if ((len & 0xC0) == 0xC0) {
            size_t ptr;
            if (pos + 1 >= packet_len) {
                return -1;
            }
            ptr = (size_t)(((len & 0x3F) << 8) | packet[pos + 1]);
            if (ptr >= packet_len || jumps++ > packet_len) {
                return -1;
            }
            if (!jumped) {
                local_next = pos + 2;
            }
            pos = ptr;
            jumped = 1;
            continue;
        }

        if ((len & 0xC0) != 0) {
            return -1;
        }

        pos++;
        if (pos + len > packet_len) {
            return -1;
        }
        if (out_pos + len + 1 >= out_len) {
            return -1;
        }

        memcpy(&out[out_pos], &packet[pos], len);
        out_pos += len;
        out[out_pos++] = '.';
        pos += len;

        if (!jumped) {
            local_next = pos;
        }
    }

    return -1;
}

static int open_browse_socket(const char *ifname, unsigned int *ifindex_out) {
    int fd;
    int yes = 1;
    int hops = 255;
    struct sockaddr_in6 bind_addr;
    struct ipv6_mreq mreq;
    unsigned int ifindex = 0;

    if (ifname != NULL) {
        ifindex = if_nametoindex(ifname);
        if (ifindex == 0) {
            return -1;
        }
    }

    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        close(fd);
        return -1;
    }

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) < 0) {
        close(fd);
        return -1;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin6_family = AF_INET6;
    bind_addr.sin6_port = htons(MDNS_PORT);
    bind_addr.sin6_addr = in6addr_any;

    if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        close(fd);
        return -1;
    }

    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET6, "ff02::fb", &mreq.ipv6mr_multiaddr) != 1) {
        close(fd);
        return -1;
    }
    mreq.ipv6mr_interface = ifindex;

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
        close(fd);
        return -1;
    }

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)) < 0) {
        close(fd);
        return -1;
    }

    if (ifindex != 0) {
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)) < 0) {
            close(fd);
            return -1;
        }
    }

    *ifindex_out = ifindex;
    return fd;
}

static int send_ptr_query(int sockfd, const char *service_type_fqdn, unsigned int ifindex) {
    uint8_t query_buf[MDNS_MAX_PACKET];
    struct sockaddr_in6 mcast_addr;
    size_t offset = 12;
    size_t qname_len;

    memset(query_buf, 0, sizeof(query_buf));
    query_buf[4] = 0;
    query_buf[5] = 1;

    if (encode_qname(service_type_fqdn, &query_buf[offset], sizeof(query_buf) - offset, &qname_len) != 0) {
        return -1;
    }
    offset += qname_len;

    if (offset + 4 > sizeof(query_buf)) {
        return -1;
    }

    query_buf[offset++] = 0;
    query_buf[offset++] = DNS_TYPE_PTR;
    query_buf[offset++] = 0;
    query_buf[offset++] = DNS_CLASS_IN;

    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin6_family = AF_INET6;
    mcast_addr.sin6_port = htons(MDNS_PORT);
    mcast_addr.sin6_scope_id = ifindex;
    if (inet_pton(AF_INET6, "ff02::fb", &mcast_addr.sin6_addr) != 1) {
        return -1;
    }

    if (sendto(sockfd, query_buf, offset, 0, (struct sockaddr *)&mcast_addr, sizeof(mcast_addr)) < 0) {
        return -1;
    }

    return 0;
}

static int parse_txt_strings(const uint8_t *rdata, size_t rdlen, char *out, size_t out_len) {
    size_t pos = 0;
    size_t out_pos = 0;

    if (out_len == 0) {
        return -1;
    }

    while (pos < rdlen) {
        uint8_t len = rdata[pos++];
        if (pos + len > rdlen) {
            return -1;
        }
        if (out_pos + len + 2 >= out_len) {
            return -1;
        }
        if (out_pos != 0) {
            out[out_pos++] = ';';
            out[out_pos++] = ' ';
        }
        memcpy(&out[out_pos], &rdata[pos], len);
        out_pos += len;
        pos += len;
    }

    out[out_pos] = '\0';
    return 0;
}

static int print_response_records(const uint8_t *packet, size_t packet_len,
                                  const struct sockaddr_in6 *src_addr,
                                  const char *service_type_fqdn) {
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
    size_t offset = 12;
    uint32_t rr_total;
    uint32_t printed = 0;
    char src_ip[INET6_ADDRSTRLEN];

    if (packet == NULL || src_addr == NULL || service_type_fqdn == NULL || packet_len < 12) {
        return -1;
    }

    if (inet_ntop(AF_INET6, &src_addr->sin6_addr, src_ip, sizeof(src_ip)) == NULL) {
        strncpy(src_ip, "<unknown>", sizeof(src_ip));
        src_ip[sizeof(src_ip) - 1] = '\0';
    }

    qdcount = read_u16(&packet[4]);
    ancount = read_u16(&packet[6]);
    nscount = read_u16(&packet[8]);
    arcount = read_u16(&packet[10]);

    for (uint16_t i = 0; i < qdcount; i++) {
        char qname[256];
        size_t next_offset;
        if (decode_name(packet, packet_len, offset, qname, sizeof(qname), &next_offset) != 0) {
            return -1;
        }
        if (next_offset + 4 > packet_len) {
            return -1;
        }
        offset = next_offset + 4;
    }

    rr_total = (uint32_t)ancount + (uint32_t)nscount + (uint32_t)arcount;
    for (uint32_t i = 0; i < rr_total; i++) {
        char name[256];
        size_t next_offset;
        uint16_t type;
        uint32_t ttl;
        uint16_t rdlen;
        const uint8_t *rdata;

        if (decode_name(packet, packet_len, offset, name, sizeof(name), &next_offset) != 0) {
            return -1;
        }
        if (next_offset + 10 > packet_len) {
            return -1;
        }

        type = read_u16(&packet[next_offset]);
        ttl = read_u32(&packet[next_offset + 4]);
        rdlen = read_u16(&packet[next_offset + 8]);
        offset = next_offset + 10;

        if (offset + rdlen > packet_len) {
            return -1;
        }

        rdata = &packet[offset];

        if (type == DNS_TYPE_PTR) {
            char ptr_name[256];
            size_t ignored_next = 0;
            if (decode_name(packet, packet_len, offset, ptr_name, sizeof(ptr_name), &ignored_next) == 0 &&
                strcasecmp(name, service_type_fqdn) == 0) {
                if (printed == 0) {
                    printf("Response from %s\n", src_ip);
                }
                printf("  PTR %s -> %s (ttl=%" PRIu32 ")\n", name, ptr_name, ttl);
                printed++;
            }
        } else if (type == DNS_TYPE_SRV && rdlen >= 6) {
            uint16_t priority = read_u16(rdata);
            uint16_t weight = read_u16(rdata + 2);
            uint16_t port = read_u16(rdata + 4);
            char target[256];
            size_t ignored_next = 0;
            if (decode_name(packet, packet_len, offset + 6, target, sizeof(target), &ignored_next) == 0) {
                if (printed == 0) {
                    printf("Response from %s\n", src_ip);
                }
                printf("  SRV %s port=%" PRIu16 " priority=%" PRIu16 " weight=%" PRIu16 " target=%s (ttl=%" PRIu32 ")\n",
                       name, port, priority, weight, target, ttl);
                printed++;
            }
        } else if (type == DNS_TYPE_TXT) {
            char txt_buf[1024];
            if (parse_txt_strings(rdata, rdlen, txt_buf, sizeof(txt_buf)) == 0) {
                if (printed == 0) {
                    printf("Response from %s\n", src_ip);
                }
                printf("  TXT %s \"%s\" (ttl=%" PRIu32 ")\n", name, txt_buf, ttl);
                printed++;
            }
        } else if (type == DNS_TYPE_A && rdlen == 4) {
            char ip[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, rdata, ip, sizeof(ip)) != NULL) {
                if (printed == 0) {
                    printf("Response from %s\n", src_ip);
                }
                printf("  A %s %s (ttl=%" PRIu32 ")\n", name, ip, ttl);
                printed++;
            }
        } else if (type == DNS_TYPE_AAAA && rdlen == 16) {
            char ip6[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, rdata, ip6, sizeof(ip6)) != NULL) {
                if (printed == 0) {
                    printf("Response from %s\n", src_ip);
                }
                printf("  AAAA %s %s (ttl=%" PRIu32 ")\n", name, ip6, ttl);
                printed++;
            }
        }

        offset += rdlen;
    }

    return (int)printed;
}

int main(int argc, char **argv) {
    browse_config_t cfg;
    int sockfd;
    unsigned int ifindex = 0;
    char service_type_fqdn[256];
    long deadline_ms;
    int total_records = 0;

    if (parse_args(argc, argv, &cfg) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    if (build_fqdn_service_type(cfg.service_type, service_type_fqdn, sizeof(service_type_fqdn)) != 0) {
        fprintf(stderr, "Invalid service type: %s\n", cfg.service_type);
        return 1;
    }

    if (log_init(cfg.verbosity, 0) != 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        return 1;
    }

    sockfd = open_browse_socket(cfg.interface_name, &ifindex);
    if (sockfd < 0) {
        if (cfg.interface_name != NULL) {
            log_error("Failed to open browse socket for interface %s", cfg.interface_name);
        } else {
            log_error("Failed to open browse socket (try --interface <ifname>)");
        }
        log_close();
        return 1;
    }

    if (cfg.verbose) {
        if (cfg.interface_name != NULL) {
            log_info("Browsing service type %s on interface %s for %d second(s)",
                     service_type_fqdn, cfg.interface_name, cfg.timeout_seconds);
        } else {
            log_info("Browsing service type %s for %d second(s)",
                     service_type_fqdn, cfg.timeout_seconds);
        }
    }

    if (send_ptr_query(sockfd, service_type_fqdn, ifindex) != 0) {
        log_error("Failed to send PTR query: %s", strerror(errno));
        close(sockfd);
        log_close();
        return 1;
    }

    printf("Query sent: PTR %s\n", service_type_fqdn);
    deadline_ms = now_ms() + (long)cfg.timeout_seconds * 1000L;

    for (;;) {
        long remaining_ms;
        fd_set rfds;
        struct timeval tv;
        int select_ret;

        remaining_ms = deadline_ms - now_ms();
        if (remaining_ms <= 0) {
            break;
        }

        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        tv.tv_sec = (time_t)(remaining_ms / 1000L);
        tv.tv_usec = (suseconds_t)((remaining_ms % 1000L) * 1000L);

        select_ret = select(sockfd + 1, &rfds, NULL, NULL, &tv);
        if (select_ret < 0) {
            log_error("select() failed: %s", strerror(errno));
            close(sockfd);
            log_close();
            return 1;
        }

        if (select_ret == 0) {
            continue;
        }

        if (FD_ISSET(sockfd, &rfds)) {
            uint8_t packet[MDNS_MAX_PACKET];
            struct sockaddr_in6 src_addr;
            socklen_t src_len = sizeof(src_addr);
            ssize_t nread;
            int printed;

            nread = recvfrom(sockfd, packet, sizeof(packet), 0,
                             (struct sockaddr *)&src_addr, &src_len);
            if (nread < 0) {
                log_warn("recvfrom() failed: %s", strerror(errno));
                continue;
            }

            printed = print_response_records(packet, (size_t)nread, &src_addr, service_type_fqdn);
            if (printed > 0) {
                total_records += printed;
            }
        }
    }

    if (total_records == 0) {
        printf("No responses for %s within %d second(s)\n", service_type_fqdn, cfg.timeout_seconds);
    }

    close(sockfd);
    log_close();
    return total_records > 0 ? 0 : 1;
}