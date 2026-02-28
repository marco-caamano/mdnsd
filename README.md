# mDNSd

`mdnsd` is a lightweight IPv6 mDNS responder with service discovery support, implemented in C99.
It binds to a selected network interface, listens on the mDNS multicast endpoint, parses incoming DNS questions, and returns A/AAAA answers for hostname resolution and SRV/TXT answers for service discovery.

## Features

- C99 implementation with strict compiler flags
- Interface-scoped IPv6 UDP socket for mDNS (`ff02::fb:5353`)
- Command-line configuration for:
  - interface (`-i`, required)
  - config file (`-c`, optional)
  - verbosity (`-v ERROR|WARN|INFO|DEBUG`)
  - logging target (`-l console|syslog`)
- Logging to console (timestamped) or syslog
- mDNS query parsing for DNS question section
- A/AAAA response generation for hostname resolution
- SRV/TXT response generation for service discovery
- Service registration API with dynamic memory allocation
- INI-style config file for service definitions
- Support for targeted and general service queries
- DNS name compression for efficient responses
- Graceful shutdown on `SIGINT` / `SIGTERM`

## Build

```bash
make
```

This builds the `mdnsd` binary in the repository root.

### Build settings

The project uses:

- C standard: `-std=c99`
- Strict warnings: `-Wall -Wextra -Werror -pedantic`
- POSIX declarations: `-D_POSIX_C_SOURCE=200809L`

## Run

```bash
./mdnsd -i <interface> [-c <config>] [-v ERROR|WARN|INFO|DEBUG] [-l console|syslog]
```

Examples:

```bash
# Run without services
./mdnsd -i eth0

# Run with service discovery
./mdnsd -i eth0 -c services.conf

# Run with debug logging
./mdnsd -i eth0 -c services.conf -v DEBUG

# Run with syslog
./mdnsd -i eth0 -c services.conf -l syslog -v INFO
```

Show help:

```bash
./mdnsd -h
```

## Design Overview

The application follows a small modular architecture:

- `main` loop orchestration
- command-line parsing
- config file parsing
- logging abstraction
- socket/multicast setup
- host and service database
- DNS/mDNS packet parsing and response construction
- service registration API

This keeps protocol logic separate from system setup and runtime control.

## Configuration File

Services are defined in an INI-style config file:

```ini
# services.conf
[service]
instance = My Web Server
type = _http._tcp
port = 8080
target = my-host.local
txt.path = /
txt.version = 1.0

[service]
instance = SSH Server
type = _ssh._tcp
port = 22
target = my-host.local
priority = 0
weight = 0
ttl = 120
```

**Required fields:**
- `instance` - Service instance name
- `type` - Service type (e.g., `_http._tcp`, `_ssh._tcp`)
- `port` - Port number (must be non-zero)
- `target` - Target hostname (e.g., `my-host.local`)

**Optional fields:**
- `domain` - Domain (default: `local`)
- `priority` - SRV priority (default: 0)
- `weight` - SRV weight (default: 0)
- `ttl` - Time to live in seconds (default: 120)
- `txt.key=value` - TXT records (multiple allowed)

## Modules

### `src/main.c`

- Parses configuration via `parse_args`
- Initializes logging (`log_init`)
- Initializes local host record (`hostdb_init`)
- Loads services from config file (`config_load_services`)
- Opens and configures mDNS socket (`mdns_socket_open`)
- Runs event loop with `select()`
- Receives datagrams with `recvfrom()`
- Parses question with `mdns_parse_query()`
- Filters supported types (A/AAAA/SRV)
- Routes A/AAAA queries via `hostdb_lookup()`
- Routes SRV queries (targeted or general) via service lookup
- Builds and sends responses with `mdns_build_response()` or `mdns_build_service_response()` + `sendto()`
- Handles shutdown signals and cleanup

### `src/args.c` + `include/args.h`

- Defines `app_config_t`:
  - `interface_name`
  - `config_path`
  - `verbosity`
  - `log_target`
- Validates required interface option
- Parses short/long options using `getopt_long`
- Prints usage/help text

### `src/config.c` + `include/config.h`

- Parses INI-style config file
- Reads `[service]` sections with key=value pairs
- Validates required fields (instance, type, port, target)
- Handles optional fields (priority, weight, ttl, domain)
- Parses TXT records via `txt.key=value` syntax
- Calls `mdns_register_service()` for each valid service
- Logs warnings for invalid entries
- Returns count of successfully loaded services

### `src/log.c` + `include/log.h`

- Provides severity levels:
  - `APP_LOG_ERROR`
  - `APP_LOG_WARN`
  - `APP_LOG_INFO`
  - `APP_LOG_DEBUG`
- Runtime verbosity filtering
- Console mode:
  - timestamped stderr lines
- Syslog mode:
  - `openlog()` / `syslog()` / `closelog()`
- Convenience macros: `log_error`, `log_warn`, `log_info`, `log_debug`

### `src/socket.c` + `include/socket.h`

- Resolves interface index (`if_nametoindex`)
- Creates IPv6 UDP socket
- Applies socket options:
  - `SO_REUSEADDR`
  - `IPV6_V6ONLY`
  - `IPV6_MULTICAST_HOPS = 255`
  - `IPV6_MULTICAST_IF`
- Binds to wildcard IPv6 on port `5353`
- Joins mDNS IPv6 multicast group `ff02::fb`

### `src/hostdb.c` + `include/hostdb.h`

- Maintains in-memory `host_record_t` with:
  - hostname
  - optional IPv4 address
  - optional IPv6 address
  - TTL value
- Maintains dynamic service list with `mdns_service_t` entries:
  - instance name
  - service type (e.g., `_http._tcp`)
  - domain
  - priority, weight, port
  - target hostname
  - TXT records (key-value pairs)
  - TTL value
- Service registration API:
  - `mdns_register_service()` - Register new service (returns error on duplicate)
  - `mdns_update_service()` - Update existing service
  - `mdns_unregister_service()` - Remove service
  - `mdns_list_services()` - List all services
- Service lookup API:
  - `mdns_find_service_by_fqdn()` - Find specific instance by full name
  - `mdns_find_services_by_type()` - Find all services of given type
- Initializes hostname from system `gethostname()` (or hint)
- Current default address data:
  - IPv4: `127.0.0.1`
  - IPv6: `::1`
- Performs normalized case-insensitive hostname match
- Dynamic memory allocation for services with proper cleanup

### `src/mdns.c` + `include/mdns.h`

- Parses DNS question section from incoming packet
- Decodes QNAME labels into dotted form
- Extracts QTYPE/QCLASS
- Supports DNS types:
  - `DNS_TYPE_A` (1) - IPv4 address
  - `DNS_TYPE_TXT` (16) - Text records
  - `DNS_TYPE_AAAA` (28) - IPv6 address
  - `DNS_TYPE_SRV` (33) - Service location
- Builds DNS response packets:
  - `mdns_build_response()` - A/AAAA records
  - `mdns_build_service_response()` - SRV+TXT records
- SRV record encoding:
  - Priority, weight, port
  - Target hostname
- TXT record encoding:
  - Length-prefixed key=value strings
  - Empty record if no TXT data
- Writes DNS header + echoed question + answer records
- Supports multiple answer records in single response
- Uses compression pointers for efficient encoding

### `Makefile`

- Builds all `src/*.c` into `build/*.o`
- Links executable `mdnsd`
- Targets:
  - `make` / `make all`
  - `make clean`
  - `make install`

## IPv6 mDNS Flow (and Code Mapping)

This section describes the network flow and where each step is implemented.

1. **Interface selection and socket setup**
   - User selects interface via `-i`.
   - Code: `parse_args()` in `src/args.c`, then `mdns_socket_open()` in `src/socket.c`.
   - Socket joins `ff02::fb` on UDP 5353 scoped to that interface.

2. **Listening for mDNS queries**
   - Daemon waits on socket readability using `select()`.
   - Code: event loop in `src/main.c`.

3. **Receiving packet**
   - Incoming datagram is read with `recvfrom()` into packet buffer.
   - Code: `src/main.c`.

4. **DNS question parsing**
   - Header and first question are parsed:
     - QNAME (e.g., `host.local.` or `_http._tcp.local.`)
     - QTYPE (`A`, `AAAA`, or `SRV`)
     - QCLASS (`IN`)
   - Code: `mdns_parse_query()` in `src/mdns.c`.

5. **Question filtering and name lookup**
   - A/AAAA/SRV queries are handled.
   - For A/AAAA: Hostname is matched against local database record.
   - For SRV: Service is looked up by instance FQDN (targeted) or service type (general).
   - Code: `is_supported_query_type()` in `src/main.c`, `hostdb_lookup()` and service lookup functions in `src/hostdb.c`.

6. **Response construction**
   - DNS response is generated with:
     - response + authoritative flags
     - echoed question
     - answer records:
       - One A or AAAA record for hostname queries
       - Multiple SRV+TXT record pairs for service queries
   - For general service queries, all matching services are returned.
   - Code: `mdns_build_response()` and `mdns_build_service_response()` in `src/mdns.c`.

7. **Response transmission**
   - Response datagram is sent via `sendto()` back to sender endpoint.
   - Code: `src/main.c`.

8. **Operational observability**
   - Each important state and error path is logged at selected verbosity.
   - Code: `src/log.c` used across modules.

## Query Support

### A/AAAA Queries (Hostname Resolution)

```bash
# Query for IPv4 address
dig @::1 -p 5353 hostname.local A

# Query for IPv6 address
dig @::1 -p 5353 hostname.local AAAA
```

### SRV Queries (Service Discovery)

**Targeted query** (specific service instance):
```bash
dig @::1 -p 5353 "My Web Server._http._tcp.local" SRV
```
Returns: Single SRV+TXT record pair for that instance

**General query** (all services of a type):
```bash
dig @::1 -p 5353 "_http._tcp.local" SRV
```
Returns: SRV+TXT record pairs for all `_http._tcp` services

## Current Scope and Notes

- Query support: A/AAAA hostname resolution and SRV/TXT service discovery
- PTR browsing responses are not implemented (use general SRV queries instead)
- Probing and conflict detection are not implemented
- Service registration via API (conflict errors on duplicates)
- Dynamic memory allocation for flexible service storage
- Host database uses loopback addresses (127.0.0.1, ::1) by default
- Designed as a minimalistic but functional mDNS responder for basic service discovery use cases
