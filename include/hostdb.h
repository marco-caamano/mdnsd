#ifndef HOSTDB_H
#define HOSTDB_H

#include <stddef.h>
#include <netinet/in.h>

typedef struct {
    char hostname[256];
    struct in_addr ipv4;
    int has_ipv4;
    struct in6_addr ipv6;
    int has_ipv6;
} host_record_t;

int hostdb_init(host_record_t *record, const char *hostname_hint);
int hostdb_lookup(const host_record_t *record, const char *qname, host_record_t *out);

#endif
