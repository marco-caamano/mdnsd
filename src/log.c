#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <syslog.h>

static log_level_t g_level = APP_LOG_WARN;
static int g_use_syslog = 0;

const char *log_level_name(log_level_t level) {
    switch (level) {
        case APP_LOG_ERROR: return "ERROR";
        case APP_LOG_WARN: return "WARN";
        case APP_LOG_INFO: return "INFO";
        case APP_LOG_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

static int to_syslog_level(log_level_t level) {
    switch (level) {
        case APP_LOG_ERROR: return LOG_ERR;
        case APP_LOG_WARN: return LOG_WARNING;
        case APP_LOG_INFO: return LOG_INFO;
        case APP_LOG_DEBUG: return LOG_DEBUG;
        default: return LOG_INFO;
    }
}

int parse_log_level(const char *value, log_level_t *level_out) {
    if (value == NULL || level_out == NULL) {
        return -1;
    }

    if (strcasecmp(value, "ERROR") == 0) {
        *level_out = APP_LOG_ERROR;
        return 0;
    }
    if (strcasecmp(value, "WARN") == 0 || strcasecmp(value, "WARNING") == 0) {
        *level_out = APP_LOG_WARN;
        return 0;
    }
    if (strcasecmp(value, "INFO") == 0) {
        *level_out = APP_LOG_INFO;
        return 0;
    }
    if (strcasecmp(value, "DEBUG") == 0) {
        *level_out = APP_LOG_DEBUG;
        return 0;
    }

    return -1;
}

int log_init(log_level_t level, int use_syslog) {
    g_level = level;
    g_use_syslog = use_syslog;

    if (g_use_syslog) {
        openlog("mdnsd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    }

    return 0;
}

void log_close(void) {
    if (g_use_syslog) {
        closelog();
    }
}

void log_message(log_level_t level, const char *fmt, ...) {
    va_list args;

    if (level > g_level) {
        return;
    }

    va_start(args, fmt);

    if (g_use_syslog) {
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        syslog(to_syslog_level(level), "%s", buffer);
    } else {
        time_t now;
        struct tm tm_now;
        char ts[32];

        now = time(NULL);
        localtime_r(&now, &tm_now);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);

        fprintf(stderr, "%s [%s] ", ts, log_level_name(level));
        vfprintf(stderr, fmt, args);
        fputc('\n', stderr);
    }

    va_end(args);
}
