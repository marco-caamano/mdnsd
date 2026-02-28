# Client Documentation

The mDNS client (`mdnsc`) is a query tool for discovering services and resolving hostnames on a local network using mDNS.

## Overview

The client performs single-shot mDNS queries for:
- Hostname resolution (A/AAAA records)
- Service discovery (SRV/TXT records)

Unlike the server which runs continuously, the client sends one query and waits for responses with a timeout.

## Usage

```bash
mdnsc [-t hostname|service|ipv4|ipv6] [-4|-6] [-v] <query-target>
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

## Examples

### Hostname Resolution

Resolve a hostname to its IPv4 and IPv6 addresses:

```bash
$ mdnsc myhost
Response from [ipv6-address]:
  Query target: myhost
  Query type: 1
```

### IPv6-Only Query

Query for IPv6 address only:

```bash
$ mdnsc -6 myhost
```

### Service Discovery

Query for a specific service by FQDN:

```bash
$ mdnsc -t service "My Web Server._http._tcp.local"
```

Discover all services of a type:

```bash
$ mdnsc -t service "_http._tcp.local"
```

### Verbose Output

Get detailed information about queries:

```bash
$ mdnsc -v myhost
[INFO] Querying for: myhost
[INFO] Query sent, waiting for responses...
Response from [ipv6-address]:
  Query target: myhost
  Query type: 1
```

### Combined Options

IPv4-only query with verbose output:

```bash
$ mdnsc -4 -v myhost
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
- Waits only 1 second for response (fixed timeout)
- Limited response parsing (prints sender only)
- Does not cache or filter multiple responses
- Only queries local link (ff02::fb scope)

## Comparison with `dig`

For more detailed DNS query results, you can use `dig`:

```bash
# Query using dig
dig @ff02::fb%eth0 -p 5353 myhost.local AAAA

# vs using mdnsc
mdnsc myhost
```

The `dig` tool provides more detailed parsing and can query specific servers, while `mdnsc` is a simpler tool focused on basic mDNS discovery.

## Troubleshooting

### "No response" when server exists

1. Verify IPv6 connectivity: `ping6 ff02::fb%eth0`
2. Check server is running and binding to the same scope
3. Try with verbose flag: `mdnsc -v <target>`
4. Test with dig: `dig @ff02::fb%eth0 -p 5353 <target>`

### Service not found

1. Query with correct service type format: `_service._protocol.local`
2. Check service is registered on server: `mdnsd -v DEBUG`
3. Query correct domain (default is `.local`)

### Permission denied

Some systems require appropriate permissions to send to multicast groups:

```bash
# For non-root users, may need:
sudo mdnsc myhost
```
