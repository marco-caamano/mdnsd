#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

// Load services from INI-style config file
// Returns number of successfully loaded services, or -1 on file open error
int config_load_services(const char *config_path);

#endif
