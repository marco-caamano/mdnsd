#include <arpa/inet.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include "args.h"
#include "hostdb.h"
#include "log.h"
#include "mdns.h"

#define MDNS_PORT 5353
#define QUERY_TIMEOUT 1  // 1 second timeout for single query

// Create and send a simple mDNS query
static int send_mdns_query(int sockfd, const char *hostname, uint16_t qtype) {
    uint8_t query_buf[MDNS_MAX_PACKET];
    struct sockaddr_in6 mcast_addr;
    size_t offset;
    ssize_t sent;

    // Build query
    memset(query_buf, 0, MDNS_MAX_PACKET);
    
    // ID = 0, QR = 0 (query), Standard query
    query_buf[0] = 0;
    query_buf[1] = 0;
    query_buf[2] = 0;
    query_buf[3] = 0;
    query_buf[4] = 0;
    query_buf[5] = 1;  // QDCOUNT = 1
    
    offset = 12;
    
    // Encode query name
    const char *cursor = hostname;
    while (*cursor != '\0') {
        const char *dot = strchr(cursor, '.');
        size_t label_len = dot ? (size_t)(dot - cursor) : strlen(cursor);
        
        if (label_len == 0 || label_len > 63) {
            return -1;
        }
        if (offset + 1 + label_len >= MDNS_MAX_PACKET) {
            return -1;
        }
        
        query_buf[offset++] = (uint8_t)label_len;
        memcpy(&query_buf[offset], cursor, label_len);
        offset += label_len;
        
        if (dot == NULL) {
            break;
        }
        cursor = dot + 1;
    }
    
    if (offset >= MDNS_MAX_PACKET) {
        return -1;
    }
    query_buf[offset++] = 0;  // Root label
    
    // Query type and class
    if (offset + 4 > MDNS_MAX_PACKET) {
        return -1;
    }
    query_buf[offset++] = (uint8_t)((qtype >> 8) & 0xFF);
    query_buf[offset++] = (uint8_t)(qtype & 0xFF);
    query_buf[offset++] = 0;  // Class IN
    query_buf[offset++] = 1;
    
    // Send to mDNS multicast group
    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin6_family = AF_INET6;
    mcast_addr.sin6_port = htons(MDNS_PORT);
    inet_pton(AF_INET6, "ff02::fb", &mcast_addr.sin6_addr);
    
    sent = sendto(sockfd, query_buf, offset, 0, (struct sockaddr *)&mcast_addr, sizeof(mcast_addr));
    if (sent < 0) {
        return -1;
    }
    
    return 0;
}

int main(int argc, char **argv) {
    client_config_t cfg;
    int sockfd;
    struct sockaddr_in6 bind_addr;
    fd_set rfds;
    struct timeval tv;

    if (parse_args(argc, argv, &cfg) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    if (log_init(cfg.verbosity, 0) != 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        return 1;
    }

    if (cfg.query_target == NULL) {
        fprintf(stderr, "Error: No query target specified\n");
        print_usage(argv[0]);
        log_close();
        return 1;
    }

    // Create UDP socket for mDNS queries
    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_error("Failed to create socket");
        log_close();
        return 1;
    }

    // Bind to any address
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin6_family = AF_INET6;
    bind_addr.sin6_port = htons(0);  // Random port
    bind_addr.sin6_addr = in6addr_any;

    if (bind(sockfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        log_error("Failed to bind socket");
        close(sockfd);
        log_close();
        return 1;
    }

    // Set multicast interface if specified
    if (cfg.interface_name != NULL) {
        unsigned int ifindex = if_nametoindex(cfg.interface_name);
        if (ifindex == 0) {
            log_error("Invalid interface: %s", cfg.interface_name);
            close(sockfd);
            log_close();
            return 1;
        }
        
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)) < 0) {
            log_error("Failed to set multicast interface");
            close(sockfd);
            log_close();
            return 1;
        }
        
        if (cfg.verbose) {
            log_info("Using interface: %s (index %u)", cfg.interface_name, ifindex);
        }
    }

    if (cfg.verbose) {
        log_info("Querying for: %s", cfg.query_target);
    }

    // Send query based on type
    uint16_t qtype = DNS_TYPE_A;
    const char *query_name = cfg.query_target;

    switch (cfg.query_type) {
        case QUERY_TYPE_HOSTNAME:
            if (cfg.ipv6_only) {
                qtype = DNS_TYPE_AAAA;
            } else if (!cfg.ipv4_only) {
                // Try both - send A first
                qtype = DNS_TYPE_A;
            }
            query_name = cfg.query_target;
            // Add .local if not present
            if (strchr(cfg.query_target, '.') == NULL) {
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s.local", cfg.query_target);
                query_name = full_name;
            }
            break;
        case QUERY_TYPE_SERVICE:
            qtype = DNS_TYPE_SRV;
            query_name = cfg.query_target;
            break;
        case QUERY_TYPE_IPv4:
            qtype = DNS_TYPE_A;
            query_name = cfg.query_target;
            break;
        case QUERY_TYPE_IPv6:
            qtype = DNS_TYPE_AAAA;
            query_name = cfg.query_target;
            break;
    }

    // Send the query
    if (send_mdns_query(sockfd, query_name, qtype) != 0) {
        log_error("Failed to send query");
        close(sockfd);
        log_close();
        return 1;
    }

    if (cfg.verbose) {
        log_info("Query sent, waiting for responses...");
    }

    // Wait for responses with timeout
    int response_count = 0;
    tv.tv_sec = QUERY_TIMEOUT;
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    int select_ret = select(sockfd + 1, &rfds, NULL, NULL, &tv);
    if (select_ret < 0) {
        log_error("Select failed");
    } else if (select_ret == 0) {
        if (cfg.verbose) {
            log_info("No responses received");
        }
        printf("No response for %s\n", cfg.query_target);
    } else if (FD_ISSET(sockfd, &rfds)) {
        // Try to receive response
        uint8_t resp_buf[MDNS_MAX_PACKET];
        struct sockaddr_in6 src_addr;
        socklen_t src_len = sizeof(src_addr);
        ssize_t nread;

        nread = recvfrom(sockfd, resp_buf, sizeof(resp_buf), 0, (struct sockaddr *)&src_addr, &src_len);
        if (nread > 0) {
            response_count++;
            if (cfg.verbose) {
                log_info("Received response (%zu bytes)", nread);
            }
            
            // Parse response and print info
            char addr_str[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &src_addr.sin6_addr, addr_str, sizeof(addr_str))) {
                printf("Response from %s:\n", addr_str);
                printf("  Query target: %s\n", cfg.query_target);
                printf("  Query type: %d\n", qtype);
            }
        }
    }

    close(sockfd);
    log_close();

    return response_count > 0 ? 0 : 1;
}
