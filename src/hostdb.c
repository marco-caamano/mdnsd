#include "hostdb.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static int normalize_local_name(const char *name, char *out, size_t out_len) {
    size_t name_len;

    if (name == NULL || out == NULL || out_len == 0) {
        return -1;
    }

    name_len = strlen(name);
    if (name_len >= out_len) {
        return -1;
    }

    memcpy(out, name, name_len + 1);

    if (name_len > 0 && out[name_len - 1] == '.') {
        out[name_len - 1] = '\0';
    }

    return 0;
}

static int append_local_suffix(char *hostname, size_t buf_len) {
    const char *suffix = ".local";
    size_t hlen = strlen(hostname);
    size_t slen = strlen(suffix);

    if (hlen >= slen && strcmp(hostname + hlen - slen, suffix) == 0) {
        return 0;
    }

    if (hlen + slen + 1 > buf_len) {
        return -1;
    }

    memcpy(hostname + hlen, suffix, slen + 1);
    return 0;
}

static void get_interface_addrs(const char *ifname, host_record_t *record) {
    struct ifaddrs *ifap, *ifa;

    if (ifname == NULL || getifaddrs(&ifap) != 0) {
        return;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || strcmp(ifa->ifa_name, ifname) != 0) {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_INET && !record->has_ipv4) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            record->ipv4 = sa->sin_addr;
            record->has_ipv4 = 1;
        } else if (ifa->ifa_addr->sa_family == AF_INET6 && !record->has_ipv6) {
            struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            record->ipv6 = sa6->sin6_addr;
            record->has_ipv6 = 1;
        }
    }

    freeifaddrs(ifap);
}

int hostdb_init(host_record_t *record, const char *hostname_hint, const char *ifname) {
    char hostbuf[256];

    if (record == NULL) {
        return -1;
    }

    memset(record, 0, sizeof(*record));

    if (hostname_hint != NULL && hostname_hint[0] != '\0') {
        if (normalize_local_name(hostname_hint, record->hostname, sizeof(record->hostname)) != 0) {
            return -1;
        }
    } else {
        if (gethostname(hostbuf, sizeof(hostbuf)) != 0) {
            return -1;
        }
        if (normalize_local_name(hostbuf, record->hostname, sizeof(record->hostname)) != 0) {
            return -1;
        }
    }

    if (append_local_suffix(record->hostname, sizeof(record->hostname)) != 0) {
        return -1;
    }

    get_interface_addrs(ifname, record);

    return 0;
}

int hostdb_lookup(const host_record_t *record, const char *qname, host_record_t *out) {
    char normalized[256];

    if (record == NULL || qname == NULL || out == NULL) {
        return -1;
    }

    if (normalize_local_name(qname, normalized, sizeof(normalized)) != 0) {
        return -1;
    }

    if (strcasecmp(normalized, record->hostname) == 0) {
        *out = *record;
        return 1;
    }

    return 0;
}
