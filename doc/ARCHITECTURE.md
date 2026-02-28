# Client Documentation

The mDNS client tools are query and browse utilities for local mDNS discovery:

- `mdns_client`: single-shot hostname/service query tool
- `mdns_browse`: service-type browser that sends PTR queries and prints responses during a timeout window

## Overview

The client performs single-shot mDNS queries for:
- Hostname resolution (A/AAAA records)
- Service discovery (SRV/TXT records)

Unlike the server which runs continuously, the client sends one query and waits for responses with a timeout.

The browser performs service-type browsing for:
- PTR records (`_service._tcp.local` style browse queries)
- Related SRV/TXT/A/AAAA records included in responses

## Usage

```bash
mdns_client [-t hostname|service|ipv4|ipv6] [-4|-6] [-v] <query-target>
```

```bash
mdns_browse -s <service-type> [-w <seconds>] [-i <interface>] [-v]
```

## Options

- `-t, --type <type>`: Query type (default: hostname)
  - `hostname`: Resolve hostname to IPv4/IPv6 address
  - `service`: Query for services
  - `ipv4`: Query for IPv4 address only
  - `ipv6`: Query for IPv6 address only
- `-4, --ipv4`: Force IPv4-only queries (A records)
- `-6, --ipv6`: Force IPv6-only queries (AAAA records)
- `-v, --verbose`: Verbose output with additional details
- `-h, --help`: Show help message

`mdns_browse` options:
- `-s, --service`: Service type to browse (required, e.g. `_http._tcp.local`)
- `-w, --timeout`: Seconds to wait for responses (default: 2)
- `-i, --interface`: Optional interface name for IPv6 multicast scope
- `-v, --verbose`: Verbose output
- `-h, --help`: Show help message

## Examples

### Hostname Resolution

Resolve a hostname to its IPv4 and IPv6 addresses:

```bash
$ mdns_client myhost
Response from [ipv6-address]:
  Query target: myhost
  Query type: 1
```

### IPv6-Only Query

Query for IPv6 address only:

```bash
$ mdns_client -6 myhost
```

### Service Discovery

Query for a specific service by FQDN:

```bash
$ mdns_client -t service "My Web Server._http._tcp.local"
```

Discover all services of a type:

```bash
$ mdns_client -t service "_http._tcp.local"
```

### Verbose Output

Get detailed information about queries:

```bash
$ mdns_client -v myhost
[INFO] Querying for: myhost
[INFO] Query sent, waiting for responses...
Response from [ipv6-address]:
  Query target: myhost
  Query type: 1
```

### Combined Options

IPv4-only query with verbose output:

```bash
$ mdns_client -4 -v myhost
```

### Service Browsing

Browse HTTP services for 5 seconds:

```bash
$ mdns_browse -s _http._tcp.local -w 5
Query sent: PTR _http._tcp.local
Response from [ipv6-address]
  PTR _http._tcp.local -> My Web Server._http._tcp.local (ttl=120)
  SRV My Web Server._http._tcp.local port=8080 priority=0 weight=0 target=my-host.local (ttl=120)
  TXT My Web Server._http._tcp.local "path=/; version=1.0" (ttl=120)
```

## Output

The client prints responses in a simple format:

```
Response from [sender-address]:
  Query target: <what was queried>
  Query type: <DNS type number>
```

If no response is received within the timeout (1 second):

```
No response for <query-target>
```

If no browse response is received within the timeout:

```
No responses for <service-type> within <seconds> second(s)
```

## Exit Codes

- `0`: Response received successfully
- `1`: No response or error occurred

## Implementation Details

### Query Process

1. Parse command-line arguments
2. Create temporary UDP socket bound to wildcard address
3. Build DNS query packet based on query type
4. Send query to mDNS multicast group (ff02::fb:5353)
5. Wait for response with 1-second timeout using `select()`
6. Print result or timeout message
7. Clean up and exit

### Browse Process (`mdns_browse`)

1. Parse service type and timeout from command-line arguments
2. Open IPv6 UDP socket on port 5353 and join multicast group `ff02::fb`
3. Send PTR query for the requested service type
4. Receive packets until timeout using `select()`
5. Parse answer, authority, and additional sections
6. Print decoded PTR/SRV/TXT/A/AAAA records
7. Clean up and exit

### DNS Query Types

- **Hostname queries**: Uses DNS type A (1) for IPv4 or AAAA (28) for IPv6
- **Service queries**: Uses DNS type SRV (33) with optional TXT (16) records
- **IPv4-only**: Ignores AAAA responses
- **IPv6-only**: Ignores A responses

### Response Handling

The client processes responses by:

1. Receiving response packet on mDNS socket
2. Parsing sender's address
3. Printing sender address and query details
4. Exiting with success

## Limitations

- Single query per invocation
- `mdns_client` waits only 1 second for response (fixed timeout)
- `mdns_browse` timeout is configurable with `-w`
- Limited response parsing (prints sender only)
- `mdns_client` does not cache or filter multiple responses
- Only queries local link (ff02::fb scope)

## Comparison with `dig`

For more detailed DNS query results, you can use `dig`:

```bash
# Query using dig
dig @ff02::fb%eth0 -p 5353 myhost.local AAAA

# vs using mdns_client
mdns_client myhost
```

The `dig` tool provides more detailed parsing and can query specific servers, while `mdns_client` is a simpler tool focused on basic mDNS discovery.

## Troubleshooting

### "No response" when server exists

1. Verify IPv6 connectivity: `ping6 ff02::fb%eth0`
2. Check server is running and binding to the same scope
3. Try with verbose flag: `mdns_client -v <target>`
4. Test with dig: `dig @ff02::fb%eth0 -p 5353 <target>`

### Service not found

1. Query with correct service type format: `_service._protocol.local`
2. Check service is registered on server: `mdns_server -v DEBUG`
3. Query correct domain (default is `.local`)

### Permission denied

Some systems require appropriate permissions to send to multicast groups:

```bash
# For non-root users, may need:
sudo mdns_client myhost
```
