#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

typedef enum {
    APP_LOG_ERROR = 0,
    APP_LOG_WARN = 1,
    APP_LOG_INFO = 2,
    APP_LOG_DEBUG = 3
} log_level_t;

int log_init(log_level_t level, int use_syslog);
void log_close(void);
void log_message(log_level_t level, const char *fmt, ...);

#define log_error(...) log_message(APP_LOG_ERROR, __VA_ARGS__)
#define log_warn(...)  log_message(APP_LOG_WARN, __VA_ARGS__)
#define log_info(...)  log_message(APP_LOG_INFO, __VA_ARGS__)
#define log_debug(...) log_message(APP_LOG_DEBUG, __VA_ARGS__)

const char *log_level_name(log_level_t level);
int parse_log_level(const char *value, log_level_t *level_out);

#endif
