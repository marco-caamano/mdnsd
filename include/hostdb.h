#ifndef HOSTDB_H
#define HOSTDB_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

typedef struct {
    char hostname[256];
    struct in_addr ipv4;
    int has_ipv4;
    struct in6_addr ipv6;
    int has_ipv6;
    uint32_t ttl;
} host_record_t;

typedef struct {
    char *instance;        // "My Web"
    char *service_type;    // "_http._tcp"
    char *domain;          // "local"
    uint16_t priority;
    uint16_t weight;
    uint16_t port;
    char *target_host;     // "my-host.local."
    char **txt_kv;         // {"path=/", "ver=1"}
    size_t txt_kv_count;
    uint32_t ttl;
} mdns_service_t;

int hostdb_init(host_record_t *record, const char *hostname_hint);
int hostdb_lookup(const host_record_t *record, const char *qname, host_record_t *out);

// Service registration API
int mdns_register_service(const mdns_service_t *svc);
int mdns_update_service(const mdns_service_t *svc);
int mdns_unregister_service(const char *instance_fqdn);
size_t mdns_list_services(mdns_service_t **out, size_t max_items);

// Service lookup API
mdns_service_t *mdns_find_service_by_fqdn(const char *fqdn);
size_t mdns_find_services_by_type(const char *service_type, const char *domain,
                                   mdns_service_t **out, size_t max_items);

// Service cleanup
void mdns_cleanup_services(void);

#endif
