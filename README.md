# mDNSd

`mdnsd` is a lightweight IPv6 mDNS responder implemented in C99.
It binds to a selected network interface, listens on the mDNS multicast endpoint, parses incoming DNS questions, and returns A/AAAA answers for a local hostname record.

## Features

- C99 implementation with strict compiler flags
- Interface-scoped IPv6 UDP socket for mDNS (`ff02::fb:5353`)
- Command-line configuration for:
  - interface (`-i`, required)
  - verbosity (`-v ERROR|WARN|INFO|DEBUG`)
  - logging target (`-l console|syslog`)
- Logging to console (timestamped) or syslog
- mDNS query parsing for DNS question section
- A/AAAA response generation with DNS name compression pointer
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
./mdnsd -i <interface> [-v ERROR|WARN|INFO|DEBUG] [-l console|syslog]
```

Examples:

```bash
./mdnsd -i eth0
./mdnsd -i eth0 -v DEBUG
./mdnsd -i eth0 -l syslog -v INFO
```

Show help:

```bash
./mdnsd -h
```

## Design Overview

The application follows a small modular architecture:

- `main` loop orchestration
- command-line parsing
- logging abstraction
- socket/multicast setup
- host lookup database
- DNS/mDNS packet parsing and response construction

This keeps protocol logic separate from system setup and runtime control.

## Modules

### `src/main.c`

- Parses configuration via `parse_args`
- Initializes logging (`log_init`)
- Initializes local host record (`hostdb_init`)
- Opens and configures mDNS socket (`mdns_socket_open`)
- Runs event loop with `select()`
- Receives datagrams with `recvfrom()`
- Parses question with `mdns_parse_query()`
- Filters supported types (A/AAAA)
- Resolves local record via `hostdb_lookup()`
- Builds and sends response with `mdns_build_response()` + `sendto()`
- Handles shutdown signals and cleanup

### `src/args.c` + `include/args.h`

- Defines `app_config_t`:
  - `interface_name`
  - `verbosity`
  - `log_target`
- Validates required interface option
- Parses short/long options using `getopt_long`
- Prints usage/help text

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
- Initializes hostname from system `gethostname()` (or hint)
- Current default address data:
  - IPv4: `127.0.0.1`
  - IPv6: `::1`
- Performs normalized case-insensitive hostname match

### `src/mdns.c` + `include/mdns.h`

- Parses DNS question section from incoming packet
- Decodes QNAME labels into dotted form
- Extracts QTYPE/QCLASS
- Builds DNS response packet for A or AAAA record
- Writes DNS header + echoed question + answer record
- Uses compression pointer (`0xC00C`) for answer name

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
     - QNAME (e.g., `host.local.`)
     - QTYPE (`A` or `AAAA`)
     - QCLASS (`IN`)
   - Code: `mdns_parse_query()` in `src/mdns.c`.

5. **Question filtering and name lookup**
   - Only A/AAAA are handled.
   - Hostname is matched against local database record.
   - Code: `is_supported_query_type()` in `src/main.c`, `hostdb_lookup()` in `src/hostdb.c`.

6. **Response construction**
   - DNS response is generated with:
     - response + authoritative flags
     - echoed question
     - one answer (A or AAAA) when available
   - Code: `mdns_build_response()` in `src/mdns.c`.

7. **Response transmission**
   - Response datagram is sent via `sendto()` back to sender endpoint.
   - Code: `src/main.c`.

8. **Operational observability**
   - Each important state and error path is logged at selected verbosity.
   - Code: `src/log.c` used across modules.

## Current Scope and Notes

- Current query scope: A/AAAA hostname resolution only.
- Service discovery records (PTR/SRV/TXT) are intentionally out of scope.
- Host database is currently minimal and in-memory.
- Designed as a simple baseline for future expansion (additional records, config-backed host database, richer mDNS behavior).
