#include "hostdb.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

// Global service list
static mdns_service_t **services = NULL;
static size_t service_count = 0;
static size_t service_capacity = 0;

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
    
    record->ttl = 120;  // Default TTL

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

// Helper: Construct service FQDN "instance.service_type.domain"
static int construct_service_fqdn(const mdns_service_t *svc, char *fqdn, size_t fqdn_len) {
    int written = snprintf(fqdn, fqdn_len, "%s.%s.%s",
                           svc->instance, svc->service_type, svc->domain);
    if (written < 0 || (size_t)written >= fqdn_len) {
        return -1;
    }
    return 0;
}

// Helper: Find service by exact FQDN match
mdns_service_t *mdns_find_service_by_fqdn(const char *fqdn) {
    char service_fqdn[512];
    
    for (size_t i = 0; i < service_count; i++) {
        if (construct_service_fqdn(services[i], service_fqdn, sizeof(service_fqdn)) == 0) {
            if (strcasecmp(fqdn, service_fqdn) == 0) {
                return services[i];
            }
        }
    }
    return NULL;
}

// Helper: Find all services matching service_type.domain
size_t mdns_find_services_by_type(const char *service_type, const char *domain,
                                   mdns_service_t **out, size_t max_items) {
    size_t found = 0;
    
    for (size_t i = 0; i < service_count && found < max_items; i++) {
        if (strcasecmp(services[i]->service_type, service_type) == 0 &&
            strcasecmp(services[i]->domain, domain) == 0) {
            out[found++] = services[i];
        }
    }
    return found;
}

// Helper: Duplicate string
static char *str_dup(const char *s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

// Helper: Validate service fields
static int validate_service(const mdns_service_t *svc) {
    if (svc == NULL) return -1;
    if (svc->instance == NULL || svc->instance[0] == '\0') return -1;
    if (svc->service_type == NULL || svc->service_type[0] == '\0') return -1;
    if (svc->domain == NULL || svc->domain[0] == '\0') return -1;
    if (svc->target_host == NULL || svc->target_host[0] == '\0') return -1;
    if (svc->port == 0) return -1;
    if (strcasecmp(svc->domain, "local") != 0) return -1;
    
    // Service type should look like _name._tcp or _name._udp
    if (svc->service_type[0] != '_') return -1;
    
    return 0;
}

// Helper: Free service memory
static void free_service(mdns_service_t *svc) {
    if (svc == NULL) return;
    
    free(svc->instance);
    free(svc->service_type);
    free(svc->domain);
    free(svc->target_host);
    
    if (svc->txt_kv != NULL) {
        for (size_t i = 0; i < svc->txt_kv_count; i++) {
            free(svc->txt_kv[i]);
        }
        free(svc->txt_kv);
    }
    
    free(svc);
}

int mdns_register_service(const mdns_service_t *svc) {
    mdns_service_t *new_service;
    char fqdn[512];
    
    if (validate_service(svc) != 0) {
        return -1;
    }
    
    // Check for duplicate
    if (construct_service_fqdn(svc, fqdn, sizeof(fqdn)) == 0) {
        if (mdns_find_service_by_fqdn(fqdn) != NULL) {
            return -1;  // Conflict error
        }
    }
    
    // Allocate new service
    new_service = calloc(1, sizeof(mdns_service_t));
    if (new_service == NULL) {
        return -1;
    }
    
    // Duplicate strings
    new_service->instance = str_dup(svc->instance);
    new_service->service_type = str_dup(svc->service_type);
    new_service->domain = str_dup(svc->domain);
    new_service->target_host = str_dup(svc->target_host);
    
    if (new_service->instance == NULL || new_service->service_type == NULL ||
        new_service->domain == NULL || new_service->target_host == NULL) {
        free_service(new_service);
        return -1;
    }
    
    // Duplicate TXT records
    if (svc->txt_kv_count > 0 && svc->txt_kv != NULL) {
        new_service->txt_kv = calloc(svc->txt_kv_count, sizeof(char *));
        if (new_service->txt_kv == NULL) {
            free_service(new_service);
            return -1;
        }
        
        for (size_t i = 0; i < svc->txt_kv_count; i++) {
            new_service->txt_kv[i] = str_dup(svc->txt_kv[i]);
            if (new_service->txt_kv[i] == NULL) {
                new_service->txt_kv_count = i;
                free_service(new_service);
                return -1;
            }
        }
        new_service->txt_kv_count = svc->txt_kv_count;
    }
    
    // Copy other fields
    new_service->priority = svc->priority;
    new_service->weight = svc->weight;
    new_service->port = svc->port;
    new_service->ttl = svc->ttl > 0 ? svc->ttl : 120;
    
    // Expand capacity if needed
    if (service_count >= service_capacity) {
        size_t new_capacity = service_capacity == 0 ? 8 : service_capacity * 2;
        mdns_service_t **new_services = realloc(services, new_capacity * sizeof(mdns_service_t *));
        if (new_services == NULL) {
            free_service(new_service);
            return -1;
        }
        services = new_services;
        service_capacity = new_capacity;
    }
    
    // Add to list
    services[service_count++] = new_service;
    return 0;
}

int mdns_update_service(const mdns_service_t *svc) {
    mdns_service_t *existing;
    char fqdn[512];
    
    if (validate_service(svc) != 0) {
        return -1;
    }
    
    if (construct_service_fqdn(svc, fqdn, sizeof(fqdn)) != 0) {
        return -1;
    }
    
    existing = mdns_find_service_by_fqdn(fqdn);
    if (existing == NULL) {
        return -1;  // Not found
    }
    
    // Update fields
    existing->priority = svc->priority;
    existing->weight = svc->weight;
    existing->port = svc->port;
    existing->ttl = svc->ttl > 0 ? svc->ttl : 120;
    
    // Update target host
    char *new_target = str_dup(svc->target_host);
    if (new_target == NULL) {
        return -1;
    }
    free(existing->target_host);
    existing->target_host = new_target;
    
    // Update TXT records
    if (existing->txt_kv != NULL) {
        for (size_t i = 0; i < existing->txt_kv_count; i++) {
            free(existing->txt_kv[i]);
        }
        free(existing->txt_kv);
        existing->txt_kv = NULL;
        existing->txt_kv_count = 0;
    }
    
    if (svc->txt_kv_count > 0 && svc->txt_kv != NULL) {
        existing->txt_kv = calloc(svc->txt_kv_count, sizeof(char *));
        if (existing->txt_kv == NULL) {
            return -1;
        }
        
        for (size_t i = 0; i < svc->txt_kv_count; i++) {
            existing->txt_kv[i] = str_dup(svc->txt_kv[i]);
            if (existing->txt_kv[i] == NULL) {
                existing->txt_kv_count = i;
                return -1;
            }
        }
        existing->txt_kv_count = svc->txt_kv_count;
    }
    
    return 0;
}

int mdns_unregister_service(const char *instance_fqdn) {
    if (instance_fqdn == NULL) {
        return -1;
    }
    
    for (size_t i = 0; i < service_count; i++) {
        char fqdn[512];
        if (construct_service_fqdn(services[i], fqdn, sizeof(fqdn)) == 0) {
            if (strcasecmp(fqdn, instance_fqdn) == 0) {
                // Free service
                free_service(services[i]);
                
                // Shift remaining services
                for (size_t j = i; j < service_count - 1; j++) {
                    services[j] = services[j + 1];
                }
                service_count--;
                return 0;
            }
        }
    }
    
    return -1;  // Not found
}

size_t mdns_list_services(mdns_service_t **out, size_t max_items) {
    if (out == NULL || max_items == 0) {
        return 0;
    }
    
    size_t count = service_count < max_items ? service_count : max_items;
    for (size_t i = 0; i < count; i++) {
        out[i] = services[i];
    }
    return count;
}

void mdns_cleanup_services(void) {
    for (size_t i = 0; i < service_count; i++) {
        free_service(services[i]);
    }
    free(services);
    services = NULL;
    service_count = 0;
    service_capacity = 0;
}
