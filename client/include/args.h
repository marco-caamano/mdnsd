#ifndef ARGS_H
#define ARGS_H

#include "log.h"

typedef enum {
    QUERY_TYPE_HOSTNAME,
    QUERY_TYPE_SERVICE,
    QUERY_TYPE_IPv4,
    QUERY_TYPE_IPv6
} query_type_t;

typedef struct {
    query_type_t query_type;
    const char *query_target;  // hostname, service FQDN, or service type
    const char *interface_name;  // network interface name (optional)
    int verbose;
    log_level_t verbosity;
    int ipv4_only;
    int ipv6_only;
} client_config_t;

int parse_args(int argc, char **argv, client_config_t *cfg);
void print_usage(const char *progname);

#endif
