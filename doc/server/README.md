# Minimal mDNS Server Design

This document defines a minimalistic mDNS responder design focused only on the core features needed for local host and service resolution.

## Scope (MVP Only)

The server supports only:

- `A` queries for `host.local.`
- `AAAA` queries for `host.local.`
- `SRV` queries for services
- `TXT` records returned together with `SRV` answers

Out of scope for this minimal design:

- `PTR` browsing responses
- Probing/conflict renaming
- Goodbye packets, advanced announcement logic
- Multi-interface policy complexity
- Dynamic ACL/rate-limit frameworks

## Minimal Architecture

Keep a single event loop and only essential modules:

1. **Socket module**
   - Open UDP `5353`
   - Join mDNS multicast (`224.0.0.251`, `ff02::fb`)
   - Receive queries and send responses

2. **Packet codec**
   - Parse DNS header + first question
   - Encode response header, question echo, answer RRs
   - Handle name encoding/compression safely

3. **Record store**
   - One host record (`hostname`, optional `A`, optional `AAAA`)
   - Service table for published services

4. **Query dispatcher**
   - Route by QTYPE (`A`, `AAAA`, `SRV`)
   - For `SRV`, append matching `TXT` answer(s)

5. **Registration API**
   - Add/update/remove published services
   - Validate names and required fields

## Data Model

### Host Record

```c
typedef struct {
    const char *hostname_fqdn;   // "my-host.local."
    bool has_ipv4;
    uint8_t ipv4[4];
    bool has_ipv6;
    uint8_t ipv6[16];
    uint32_t ttl;
} mdns_host_record_t;
```

### Service Record

```c
typedef struct {
    const char *instance;        // "My Web"
    const char *service_type;    // "_http._tcp"
    const char *domain;          // "local"
    uint16_t priority;
    uint16_t weight;
    uint16_t port;
    const char *target_host;     // "my-host.local."
    const char **txt_kv;         // {"path=/", "ver=1"}
    size_t txt_kv_count;
    uint32_t ttl;
} mdns_service_t;
```

## Required Query Behavior

### 1) A Query

- If QNAME matches configured host and IPv4 exists, return one `A` answer.

### 2) AAAA Query

- If QNAME matches configured host and IPv6 exists, return one `AAAA` answer.

### 3) SRV Query (Targeted Service Query)

Targeted query means QNAME is a specific instance FQDN, for example:

- `My Web._http._tcp.local.`

Behavior:

- Return one `SRV` answer for that instance
- Return matching `TXT` answer for same owner name
- Optionally include target host `A/AAAA` in Additional section

### 4) SRV Query (General Service Query)

General query means QNAME is a service type FQDN, for example:

- `_http._tcp.local.`

Behavior in this minimal server:

- Find all registered instances with `service_type == _http._tcp` and `domain == local`
- Return one `SRV` answer per matching instance
- Return corresponding `TXT` answer per instance
- If none match, return no answers

This gives basic service enumeration without implementing `PTR` in the MVP.

## Service Registration API

Server must expose a way to register services it publishes.

### C API

```c
int mdns_register_service(const mdns_service_t *svc);
int mdns_update_service(const mdns_service_t *svc);
int mdns_unregister_service(const char *instance_fqdn);
size_t mdns_list_services(mdns_service_t *out, size_t max_items);
```

### Validation Rules

- `instance`, `service_type`, `domain`, `target_host` are required
- `service_type` should look like `_name._tcp` or `_name._udp`
- `domain` must be `local`
- `port` must be non-zero
- Duplicate instance FQDN updates existing entry or returns conflict (pick one policy and keep it consistent)

### Minimal Registration Flow

1. Validate input fields
2. Normalize FQDN strings
3. Insert/update service in in-memory table
4. Service becomes immediately available for `SRV` queries

## Response Construction Rules

- Set response flags for authoritative answer
- Echo original question in response
- For `SRV` responses, always include paired `TXT`
- Keep default TTL simple (for example `120` seconds)
- Drop malformed packets silently or log at debug level

## Suggested Incremental Implementation Order

1. `A` + `AAAA` exact host answers
2. Service table + registration API
3. Targeted `SRV` + paired `TXT`
4. General `SRV` query returning all matching instances
5. Optional additional `A/AAAA` in `SRV` responses

---

## Notes for This Repository (`mdnsd`)

Given current module split (`socket`, `mdns`, `hostdb`, `main`), a minimal path is:

- Extend `hostdb` to store a small array/vector of `mdns_service_t`
- Add `SRV` and `TXT` encoding support in `mdns` module
- Route `SRV` queries in `main` through two paths:
  - targeted instance query
  - general service-type query
- Add registration entry points (CLI/config or function API)
