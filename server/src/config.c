#include "config.h"
#include "hostdb.h"
#include "log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_TXT_RECORDS 32

// Trim whitespace from both ends of a string
static char *trim(char *str) {
    char *end;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

// Parse a service section from config file
// Returns number of successfully registered services
// Sets g_pending_section_line if another section header is encountered
static char g_pending_line[MAX_LINE] = {0};
static int g_has_pending = 0;

static int parse_service_section(FILE *fp, int *line_num) {
    mdns_service_t svc;
    char line[MAX_LINE];
    char *txt_records[MAX_TXT_RECORDS];
    size_t txt_count = 0;
    int has_instance = 0;
    int has_type = 0;
    int has_port = 0;
    int has_target = 0;
    
    memset(&svc, 0, sizeof(svc));
    svc.domain = "local";
    svc.priority = 0;
    svc.weight = 0;
    svc.ttl = 120;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        (*line_num)++;
        char *trimmed = trim(line);
        
        // Empty line or comment
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }
        
        // New section starts - save it and return
        if (trimmed[0] == '[') {
            strcpy(g_pending_line, line);
            g_has_pending = 1;
            break;
        }
        
        // Parse key=value
        char *eq = strchr(trimmed, '=');
        if (eq == NULL) {
            log_warn("Config line %d: invalid format (no '=')", *line_num);
            continue;
        }
        
        *eq = '\0';
        char *key = trim(trimmed);
        char *value = trim(eq + 1);
        
        if (strcmp(key, "instance") == 0) {
            svc.instance = malloc(strlen(value) + 1);
            if (svc.instance) strcpy(svc.instance, value);
            has_instance = 1;
        } else if (strcmp(key, "type") == 0) {
            svc.service_type = malloc(strlen(value) + 1);
            if (svc.service_type) strcpy(svc.service_type, value);
            has_type = 1;
        } else if (strcmp(key, "port") == 0) {
            svc.port = (uint16_t)atoi(value);
            has_port = 1;
        } else if (strcmp(key, "target") == 0) {
            svc.target_host = malloc(strlen(value) + 1);
            if (svc.target_host) strcpy(svc.target_host, value);
            has_target = 1;
        } else if (strcmp(key, "priority") == 0) {
            svc.priority = (uint16_t)atoi(value);
        } else if (strcmp(key, "weight") == 0) {
            svc.weight = (uint16_t)atoi(value);
        } else if (strcmp(key, "ttl") == 0) {
            svc.ttl = (uint32_t)atoi(value);
        } else if (strncmp(key, "txt.", 4) == 0) {
            // TXT record: txt.key=value -> store as "key=value"
            if (txt_count < MAX_TXT_RECORDS) {
                size_t txt_len = strlen(key + 4) + 1 + strlen(value) + 1;
                char *txt = malloc(txt_len);
                if (txt != NULL) {
                    snprintf(txt, txt_len, "%s=%s", key + 4, value);
                    txt_records[txt_count++] = txt;
                }
            }
        } else if (strcmp(key, "domain") == 0) {
            svc.domain = malloc(strlen(value) + 1);
            if (svc.domain) strcpy(svc.domain, value);
        } else {
            log_warn("Config line %d: unknown key '%s'", *line_num, key);
        }
    }
    
    // Validate required fields
    if (!has_instance || !has_type || !has_port || !has_target) {
        log_warn("Config: incomplete service definition (missing required fields)");
        free(svc.instance);
        free(svc.service_type);
        free(svc.target_host);
        free(svc.domain);
        for (size_t i = 0; i < txt_count; i++) {
            free(txt_records[i]);
        }
        return 0;
    }
    
    // Set TXT records
    svc.txt_kv = txt_records;
    svc.txt_kv_count = txt_count;
    
    // Register service
    int result = mdns_register_service(&svc);
    if (result != 0) {
        log_warn("Config: failed to register service '%s.%s.%s'",
                 svc.instance, svc.service_type, svc.domain);
        free(svc.instance);
        free(svc.service_type);
        free(svc.target_host);
        free(svc.domain);
        for (size_t i = 0; i < txt_count; i++) {
            free(txt_records[i]);
        }
        return 0;
    }
    
    log_info("Registered service: %s.%s.%s:%d",
             svc.instance, svc.service_type, svc.domain, svc.port);
    
    return 1;
}

int config_load_services(const char *config_path) {
    FILE *fp;
    char line[MAX_LINE];
    int line_num = 0;
    int service_count = 0;
    
    // Reset pending state
    g_has_pending = 0;
    
    if (config_path == NULL) {
        return 0;
    }
    
    fp = fopen(config_path, "r");
    if (fp == NULL) {
        log_error("Failed to open config file: %s", config_path);
        return -1;
    }
    
    while (1) {
        // Use pending line if available, otherwise read new line
        if (g_has_pending) {
            strcpy(line, g_pending_line);
            g_has_pending = 0;
        } else if (fgets(line, sizeof(line), fp) == NULL) {
            break;
        } else {
            line_num++;
        }
        
        char *trimmed = trim(line);
        
        // Empty line or comment
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }
        
        // Section header
        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (end == NULL) {
                log_warn("Config line %d: unclosed section header", line_num);
                continue;
            }
            
            *end = '\0';
            char *section = trim(trimmed + 1);
            
            if (strcmp(section, "service") == 0) {
                service_count += parse_service_section(fp, &line_num);
            } else {
                log_warn("Config line %d: unknown section '%s'", line_num, section);
            }
        } else {
            log_warn("Config line %d: unexpected content outside section", line_num);
        }
    }
    
    fclose(fp);
    
    log_info("Loaded %d service(s) from config", service_count);
    return service_count;
}
