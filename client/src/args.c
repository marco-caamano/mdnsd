#include "args.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char *progname) {
    fprintf(stderr,
            "mDNS Client - Query mDNS for hostnames and services\n\n"
            "Usage: %s [options] <query>\n"
            "       %s -t service <service-type> [-4|-6] [-v]\n\n"
            "Options:\n"
            "  <query>           Hostname or service FQDN to resolve (default: A/AAAA lookup)\n"
            "  -t, --type        Query type: hostname|service|ipv4|ipv6 (default: hostname)\n"
            "  -i, --interface   Network interface name (optional)\n"
            "  -4, --ipv4        IPv4 only (A records)\n"
            "  -6, --ipv6        IPv6 only (AAAA records)\n"
            "  -v, --verbose     Verbose output\n"
            "  -h, --help        Show this help\n",
            progname, progname);
}

int parse_args(int argc, char **argv, client_config_t *cfg) {
    static struct option long_opts[] = {
        {"type", required_argument, 0, 't'},
        {"interface", required_argument, 0, 'i'},
        {"ipv4", no_argument, 0, '4'},
        {"ipv6", no_argument, 0, '6'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;

    if (cfg == NULL) {
        return -1;
    }

    // Set defaults
    cfg->query_type = QUERY_TYPE_HOSTNAME;
    cfg->query_target = NULL;
    cfg->interface_name = NULL;
    cfg->verbose = 0;
    cfg->verbosity = APP_LOG_WARN;
    cfg->ipv4_only = 0;
    cfg->ipv6_only = 0;

    while ((opt = getopt_long(argc, argv, "t:i:46vh", long_opts, NULL)) != -1) {
        switch (opt) {
            case 't':
                if (strcmp(optarg, "hostname") == 0) {
                    cfg->query_type = QUERY_TYPE_HOSTNAME;
                } else if (strcmp(optarg, "service") == 0) {
                    cfg->query_type = QUERY_TYPE_SERVICE;
                } else if (strcmp(optarg, "ipv4") == 0) {
                    cfg->query_type = QUERY_TYPE_IPv4;
                } else if (strcmp(optarg, "ipv6") == 0) {
                    cfg->query_type = QUERY_TYPE_IPv6;
                } else {
                    fprintf(stderr, "Invalid query type: %s\n", optarg);
                    return -1;
                }
                break;
            case 'i':
                cfg->interface_name = optarg;
                break;
            case '4':
                cfg->ipv4_only = 1;
                break;
            case '6':
                cfg->ipv6_only = 1;
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

    // Get the query target (positional argument)
    if (optind < argc) {
        cfg->query_target = argv[optind];
    } else if (cfg->query_type != QUERY_TYPE_SERVICE || optind + 1 >= argc) {
        fprintf(stderr, "Missing query target\n");
        return -1;
    } else {
        cfg->query_target = argv[optind + 1];
    }

    if (cfg->ipv4_only && cfg->ipv6_only) {
        fprintf(stderr, "Cannot specify both -4 and -6\n");
        return -1;
    }

    return 0;
}
