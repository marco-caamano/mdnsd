# Server Documentation

The mDNS server (`mdnsd`) is a responder that listens on a network interface and answers mDNS queries for hostnames and services.

## Architecture

The server consists of the following components:

### Core Components (Shared)
- **log**: Logging system with multiple severity levels
- **mdns**: DNS protocol parsing and response building
- **hostdb**: Service and hostname database

### Server-Specific Components
- **main**: Event loop and query handler
- **args**: Command-line argument parsing
- **config**: INI configuration file parser
- **socket**: IPv6 mDNS socket setup and multicast handling

## Startup Sequence

1. Parse command-line arguments
2. Initialize logging system
3. Initialize host record database
4. Load service definitions from config file
5. Create and configure mDNS socket
6. Register signal handlers (SIGINT, SIGTERM)
7. Enter event loop

## Event Loop

The server uses `select()` to wait for events on the mDNS socket:

1. Wait for incoming packet with 1-second timeout
2. On packet received:
   - Parse DNS question
   - Validate query type (A, AAAA, or SRV)
   - Look up answer in database
   - Build response packet
   - Send response to querier
3. On signal received:
   - Clean up resources
   - Exit

## Query Handling

### A/AAAA Queries (Hostname Resolution)

When a query arrives for a hostname:

1. Extract the QNAME from the question
2. Normalize and match against local hostname
3. If match found:
   - Build response packet with A or AAAA records
   - Send response

### SRV Queries (Service Discovery)

When a query arrives for a service:

1. Extract the QNAME from the question
2. Determine query type:
   - **Targeted query**: Service instance FQDN (e.g., `My Web._http._tcp.local.`)
     - Look up exact service by FQDN
     - Return single SRV+TXT record pair
   - **General query**: Service type (e.g., `_http._tcp.local.`)
     - Find all services of matching type
     - Return SRV+TXT record pairs for all matches
3. Build response packet
4. Send response

## Configuration File

Services are registered via an INI-style configuration file with `[service]` sections.

### Example Configuration

```ini
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
```

### Field Description

**Required:**
- `instance`: Display name for the service
- `type`: Underscore-prefixed service type (_proto._tcp or _proto._udp)
- `port`: Port number where service is available
- `target`: Target hostname (typically hostname.local)

**Optional:**
- `domain`: DNS domain, default is "local"
- `priority`: SRV priority, default is 0
- `weight`: SRV weight, default is 0
- `ttl`: Time-to-live in seconds, default is 120
- `txt.key`: TXT record entries (multiple allowed)

## Logging

The server supports both console and syslog logging with configurable verbosity:

- **ERROR**: Errors that prevent operation
- **WARN**: Warnings about configuration or query issues
- **INFO**: Informational messages about service registration and queries
- **DEBUG**: Detailed debugging information

### Console Logging

Timestamped output to stderr:
```
2025-02-28 14:32:10 [INFO] mdnsd started on interface eth0 for host myhost
2025-02-28 14:32:10 [INFO] Registered service: Web._http._tcp.local:8080
```

### Syslog Logging

Sends messages to system syslog with LOG_DAEMON facility.

## Service Registration API

The server can programmatically register, update, and unregister services using the hostdb API:

```c
mdns_service_t service = {
    .instance = "My Service",
    .service_type = "_http._tcp",
    .domain = "local",
    .port = 8080,
    .target = "host.local",
    .priority = 0,
    .weight = 0,
    .ttl = 120
};

mdns_register_service(&service);
```

## Performance Considerations

- Uses `select()` for efficient I/O multiplexing
- Single-threaded design suitable for light to moderate workloads
- Multicast responses may require tuning TTL/multicast scope settings
- Service list is in-memory with dynamic allocation

## Limitations

- No probing or conflict detection
- No rapid response retransmission mechanism
- No multicast suppression
- Host database limited to loopback addresses by default
- Single interface per instance (run multiple instances for multiple interfaces)

## Troubleshooting

### Server not responding

1. Check interface name: `ip link show`
2. Verify IPv6 connectivity: `ping6 ff02::fb%eth0`
3. Check firewall rules for UDP port 5353
4. Enable debug logging: `-v DEBUG`

### Services not visible

1. Verify config file syntax
2. Check required fields are set
3. Look for configuration parsing warnings in logs
4. Query with dig/nslookup to test
