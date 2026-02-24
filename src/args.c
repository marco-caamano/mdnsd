#include "args.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char *progname) {
    fprintf(stderr,
            "Usage: %s -i <interface> [-v <ERROR|WARN|INFO|DEBUG>] [-l <console|syslog>]\n"
            "Options:\n"
            "  -i, --interface   Network interface name (required)\n"
            "  -v, --verbosity   Log verbosity level (default: WARN)\n"
            "  -l, --log         Log target: console or syslog (default: console)\n"
            "  -h, --help        Show this help\n",
            progname);
}

int parse_args(int argc, char **argv, app_config_t *cfg) {
    static struct option long_opts[] = {
        {"interface", required_argument, 0, 'i'},
        {"verbosity", required_argument, 0, 'v'},
        {"log", required_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;

    if (cfg == NULL) {
        return -1;
    }

    cfg->interface_name = NULL;
    cfg->verbosity = APP_LOG_WARN;
    cfg->log_target = LOG_TARGET_CONSOLE;

    while ((opt = getopt_long(argc, argv, "i:v:l:h", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'i':
                cfg->interface_name = optarg;
                break;
            case 'v':
                if (parse_log_level(optarg, &cfg->verbosity) != 0) {
                    fprintf(stderr, "Invalid verbosity level: %s\n", optarg);
                    return -1;
                }
                break;
            case 'l':
                if (strcmp(optarg, "console") == 0) {
                    cfg->log_target = LOG_TARGET_CONSOLE;
                } else if (strcmp(optarg, "syslog") == 0) {
                    cfg->log_target = LOG_TARGET_SYSLOG;
                } else {
                    fprintf(stderr, "Invalid log target: %s\n", optarg);
                    return -1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                return -1;
        }
    }

    if (cfg->interface_name == NULL) {
        fprintf(stderr, "Missing required interface option\n");
        return -1;
    }

    return 0;
}
