# mdns_server

A lightweight IPv6 mDNS (Multicast DNS) implementation in C99, separated into **server** and **client** applications:

- **Server (`mdns_server`)**: Responder that listens on a network interface and responds to mDNS queries
- **Client (`mdns_client`)**: Query tool for discovering services and resolving hostnames via mDNS
- **Browser (`mdns_browse`)**: Service browser that sends PTR queries and prints discovered responses

## Features

### Shared Components
- C99 implementation with strict compiler flags  
- Core DNS/mDNS packet parsing and response building
- IPv6 mDNS support (`ff02::fb:5353`)
- Unified logging system
- Service and host database management

### Server Features
- Interface-scoped IPv6 UDP socket for mDNS listening
- Service discovery responder (A/AAAA and SRV/TXT records)
- INI-style config file for service definitions
- Dynamic service registration API
- Graceful shutdown on `SIGINT`/`SIGTERM`
- Console and syslog logging targets

### Client Features
- Query mDNS for hostnames (A/AAAA records)
- Discover services (SRV/TXT records)
- IPv4 and IPv6 filtering options
- Timeout-based query responses

## Build

```bash
make
```

This builds both `mdns_server` (server) and `mdns_client` (client) binaries in the repository root.
It also builds `mdns_browse` (service browser).

### Build Settings

The project uses:

- C standard: `-std=c99`
- Strict warnings: `-Wall -Wextra -Werror -pedantic`
- POSIX declarations: `-D_POSIX_C_SOURCE=200809L`

## Project Structure

```
.
├── shared/              # Shared code (server and client)
│   ├── include/
│   │   ├── log.h
│   │   ├── mdns.h       # Core DNS/mDNS protocol
│   │   └── hostdb.h     # Data structures
│   └── src/
│       ├── log.c
│       ├── mdns.c
│       └── hostdb.c
├── server/              # Server implementation
│   ├── include/
│   │   ├── args.h
│   │   ├── config.h
│   │   └── socket.h
│   └── src/
│       ├── mdns_server.c
│       ├── args.c
│       ├── config.c
│       └── socket.c
├── client/              # Client implementation
│   ├── include/
│   │   └── args.h
│   └── src/
│       ├── mdns_client.c
│       └── args.c
└── doc/                 # Documentation
    ├── mdns/
    └── server/
```

## Server: `mdns_server`

The mDNS responder listens on a network interface and responds to queries.

### Usage

```bash
mdns_server -i <interface> [-c <config>] [-v ERROR|WARN|INFO|DEBUG] [-l console|syslog]
```

### Options

- `-i, --interface` (required): Network interface name
- `-c, --config`: Config file path for service definitions
- `-v, --verbosity`: Log verbosity level (default: WARN)
- `-l, --log`: Log target: console or syslog (default: console)
- `-h, --help`: Show help

### Examples

```bash
# Run without services
mdns_server -i eth0

# Run with service discovery
mdns_server -i eth0 -c services.conf

# Run with debug logging
mdns_server -i eth0 -c services.conf -v DEBUG

# Run with syslog
mdns_server -i eth0 -c services.conf -l syslog -v INFO
```

### Configuration

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

## Client: `mdns_client`

The mDNS client queries for hostnames and services on the local network.

### Usage

```bash
mdns_client [-t hostname|service|ipv4|ipv6] [-4|-6] [-v] <query-target>
```

### Options

- `-t, --type`: Query type (default: hostname)
  - `hostname` - resolve hostname to address (A/AAAA)
  - `service` - discover service by FQDN or type
  - `ipv4` - query for IPv4 address only
  - `ipv6` - query for IPv6 address only
- `-4, --ipv4`: IPv4 only (A records)
- `-6, --ipv6`: IPv6 only (AAAA records)
- `-v, --verbose`: Verbose output
- `-h, --help`: Show help

### Examples

```bash
# Resolve a hostname
mdns_client myhost

# Query for a specific service
mdns_client -t service "My Web Server._http._tcp.local"

# Discover all HTTP services
mdns_client -t service "_http._tcp.local"

# IPv6 only query
mdns_client -6 myhost

# Verbose output
mdns_client -v myhost
```

## Browser: `mdns_browse`

The browser client sends a PTR query for a service type and listens for responses during a configurable timeout window.

### Usage

```bash
mdns_browse -s <service-type> [-w <seconds>] [-i <interface>] [-p ipv4|ipv6|both] [-v]
```

### Options

- `-s, --service` (required): Service type to browse (e.g., `_http._tcp.local`)
- `-w, --timeout`: Seconds to wait for responses (default: 2)
- `-i, --interface`: Network interface name (optional, e.g., `eth0`)
- `-p, --protocol`: Browse protocol family (`ipv4`, `ipv6`, or `both`; default: `both`)
- `-v, --verbose`: Verbose output
- `-h, --help`: Show help

### Examples

```bash
# Browse HTTP services for 5 seconds
mdns_browse -s _http._tcp.local -w 5

# Browse HTTP services over IPv4 only
mdns_browse -s _http._tcp.local -p ipv4

# Browse HTTP services over IPv6 only
mdns_browse -s _http._tcp.local -p ipv6

# Browse SSH services on interface eth0
mdns_browse -s _ssh._tcp.local -i eth0 -p both
```

## Installation

```bash
make install
```

This installs `mdns_server` and `mdns_client` to `/usr/local/bin/`.
It also installs `mdns_browse` to `/usr/local/bin/`.

## Uninstallation

```bash
make uninstall
```

## Module Overview

### Shared Modules

#### `shared/log.c` + `shared/include/log.h`

Provides severity levels (ERROR, WARN, INFO, DEBUG) with:
- Runtime verbosity filtering
- Console mode: timestamped stderr lines
- Syslog mode: openlog/syslog/closelog integration
- Convenience macros: `log_error`, `log_warn`, `log_info`, `log_debug`

#### `shared/mdns.c` + `shared/include/mdns.h`

Core DNS/mDNS protocol handling:
- Parses DNS question sections from incoming packets
- Decodes QNAME labels and extracts QTYPE/QCLASS
- Supports DNS types: A (1), TXT (16), AAAA (28), SRV (33)
- Builds DNS response packets with multiple answer records
- Encodes SRV records (priority, weight, port, target)
- Encodes TXT records (length-prefixed key=value strings)
- Uses compression pointers for efficient encoding

#### `shared/hostdb.c` + `shared/include/hostdb.h`

In-memory database for hosts and services:
- **host_record_t**: hostname, IPv4, IPv6 addresses with TTL
- **mdns_service_t**: instance, service type, domain, priority, weight, port, target, TXT records, TTL
- Service registration API: register, update, unregister, list, lookup
- Performs case-insensitive hostname matching
- Supports dynamic memory allocation with proper cleanup

### Server-Specific Modules

#### `server/src/mdns_server.c`

Event loop and query handler:
- Initializes configuration via `parse_args`
- Sets up logging and host database
- Opens mDNS socket and loads services
- Waits for incoming packets with `select()`
- Parses questions and routes responses
- Handles A/AAAA (hostname) and SRV (service) queries
- Sends responses and manages shutdown

#### `server/src/args.c` + `server/include/args.h`

Server argument parsing:
- Interface (`-i`, required)
- Config file (`-c`, optional)
- Verbosity (`-v`)
- Log target (`-l`)
- Validates required interface option

#### `server/src/config.c` + `server/include/config.h`

INI config file parsing:
- Reads `[service]` sections
- Validates required fields (instance, type, port, target)
- Handles optional fields (priority, weight, ttl, domain)
- Parses TXT records via `txt.key=value` syntax
- Registers services and logs results

#### `server/src/socket.c` + `server/include/socket.h`

IPv6 mDNS socket setup:
- Resolves interface index from name
- Creates IPv6 UDP socket with proper socket options
- Binds to port 5353 and joins mDNS multicast group `ff02::fb`
- Sets multicast TTL and interface

### Client-Specific Modules

#### `client/src/mdns_client.c`

Query dispatcher:
- Creates temporary mDNS socket
- Sends single query based on type (A/AAAA/SRV)
- Waits for response with timeout
- Prints results or "no response"

#### `client/src/args.c` + `client/include/args.h`

Client argument parsing:
- Query type (hostname, service, ipv4, ipv6)
- IPv4/IPv6 filtering
- Verbose mode
- Positional query target argument

#### `client/src/mdns_browse.c`

Service browsing client:
- Sends PTR browse queries for service types (for example `_http._tcp.local`)
- Waits for responses up to a configurable timeout (`-w`)
- Parses and prints PTR, SRV, TXT, A, and AAAA records
- Supports optional IPv6 multicast interface selection (`-i`)

## Query Examples

### Server: Using dig for queries

```bash
# Query for IPv4 address
dig @::1 -p 5353 hostname.local A

# Query for IPv6 address
dig @::1 -p 5353 hostname.local AAAA

# Query specific service
dig @::1 -p 5353 "My Web Server._http._tcp.local" SRV

# Query all services of a type
dig @::1 -p 5353 "_http._tcp.local" SRV
```

## Scope and Limitations

- Query support: A/AAAA hostname resolution and SRV/TXT service discovery
- Service-type browsing: PTR via `mdns_browse`
- Probing and conflict detection are not implemented
- Host database uses loopback addresses (127.0.0.1, ::1) by default
- Designed as a minimalistic mDNS implementation for basic service discovery

## Additional Documentation

See the [doc/](doc/) directory for:
- [doc/mdns/protocol-overview.md](doc/mdns/protocol-overview.md) - mDNS protocol overview
- [doc/mdns/packet-format.md](doc/mdns/packet-format.md) - DNS packet format
- [doc/mdns/message-types.md](doc/mdns/message-types.md) - DNS message types
- [doc/mdns/multicast-groups.md](doc/mdns/multicast-groups.md) - mDNS multicast groups
- [doc/mdns/a-record-exchange.md](doc/mdns/a-record-exchange.md) - A record exchange example
