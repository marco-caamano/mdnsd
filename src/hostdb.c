#include "hostdb.h"

#include <arpa/inet.h>
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

int hostdb_init(host_record_t *record, const char *hostname_hint) {
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
        hostbuf[sizeof(hostbuf) - 1] = '\0';
        if (normalize_local_name(hostbuf, record->hostname, sizeof(record->hostname)) != 0) {
            return -1;
        }
    }

    if (inet_pton(AF_INET, "127.0.0.1", &record->ipv4) == 1) {
        record->has_ipv4 = 1;
    }
    if (inet_pton(AF_INET6, "::1", &record->ipv6) == 1) {
        record->has_ipv6 = 1;
    }

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
