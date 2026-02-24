#ifndef ARGS_H
#define ARGS_H

#include "log.h"

typedef enum {
    LOG_TARGET_CONSOLE = 0,
    LOG_TARGET_SYSLOG = 1
} log_target_t;

typedef struct {
    const char *interface_name;
    log_level_t verbosity;
    log_target_t log_target;
} app_config_t;

int parse_args(int argc, char **argv, app_config_t *cfg);
void print_usage(const char *progname);

#endif
