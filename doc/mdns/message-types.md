# mDNS Message Types and Record Formats

This document covers the different types of mDNS messages, their purposes, and the resource record types they contain.

## Table of Contents

- [Message Types](#message-types)
- [Resource Record (RR) Types](#resource-record-rr-types)
- [Query and Response Scenarios](#query-and-response-scenarios)
- [Response Rules (RFC 6762)](#response-rules-rfc-6762)
- [Summary Table: Common Record Types](#summary-table-common-record-types)

## Message Types

mDNS uses DNS queries and responses, differentiated by the **QR (Query/Response)** flag in the header.

### Query Messages (QR = 0)

**Purpose**: A device sends a query to ask other devices for information about a specific name/service.

#### Standard Query Format

```
DNS Header Flags (QR=0):
  Bit 0: QR = 0 (Query)
  Bit 1-4: Opcode = 0 (Standard query)
  Bit 5: AA = 0 (Not applicable for queries)
  Bit 6: TC = 0 (Not truncated)
  Bit 7: RD = 0 (Recursion not desired in mDNS)
  Bit 8: RA = 0 (Not applicable for queries)
  Bit 9: Z = 0 (Reserved, must be 0)
  Bit 10-15: RCODE = 0 (Not applicable for queries)
  
Flags Value: 0x0000
```

#### Query Contents

A query message contains:
- **Question Section**: One or more questions (typical: ask for one name)
- **Known-Answer Suppression List** (optional): Previously known answers (Answer section)
- **Authority/Additional Sections**: Usually empty in standard queries

#### Query Examples

**Example 1: Simple A Record Query**
```
Question: What is the IPv4 address of "printer.local"?

Header:
  TransID: 0x0000
  Flags: 0x0000 (query)
  QCount: 1
  ACount: 0

Question Section:
  QNAME: printer.local
  QTYPE: 1 (A record)
  QCLASS: 0x0001 (IN, multicast response OK)
```

**Example 2: Query with Known-Answer Suppression**
```
Question: What is the IPv4 address of "homeserver.local"?
          (But I already know it's 192.168.1.100)

Header:
  TransID: 0x0000
  Flags: 0x0000 (query)
  QCount: 1
  ACount: 1  <- Include known answer

Question Section:
  QNAME: homeserver.local
  QTYPE: 1 (A record)
  QCLASS: 0x0001 (IN)

Answer Section (Known-Answer Suppression):
  NAME: homeserver.local
  TYPE: 1 (A record)
  CLASS: 0x0001 (IN)
  TTL: 120
  RDLEN: 4
  RDATA: 192.168.1.100
```

**Example 3: Unicast Response Requested**
```
Question: What is the IPv4 address of "homeserver.local"?
          (Please send response via unicast, not multicast)

Header:
  TransID: 0x1234 (non-zero for unicast matching)
  Flags: 0x0000 (query)
  QCount: 1
  ACount: 0

Question Section:
  QNAME: homeserver.local
  QTYPE: 1 (A record)
  QCLASS: 0x8001 (IN + unicast bit set = response desired via unicast)
```

### Response Messages (QR = 1)

**Purpose**: A device responds to a query with answer records it has authority over.

#### Authoritative Response Format

```
DNS Header Flags (QR=1, AA=1):
  Bit 0: QR = 1 (Response)
  Bit 1-4: Opcode = 0 (Standard query response)
  Bit 5: AA = 1 (Authoritative - responder has authority)
  Bit 6: TC = 0 (Not truncated)
  Bit 7: RD = 0 (Recursion not desired)
  Bit 8: RA = 0 (Recursion not available)
  Bit 9: Z = 0 (Reserved)
  Bit 10-15: RCODE = 0 (No error)
  
Flags Value: 0x8400
```

#### Response Contents

A standard response message contains:
- **Question Section**: Echo of the original question(s) (for clarity)
- **Answer Section**: Resource records answering the question(s)
- **Authority Section**: Authority records (typically empty in mDNS)
- **Additional Section**: Additional useful information (optional)

#### Response Types

`AA` indicates **authoritative answer**.

| Type | AA Flag | Multicast/Unicast | Purpose |
|------|---------|-------------------|---------|
| **Unicast Response** | Typically 1 in mDNS | Unicast to querier | Direct answer sent only to the requester |
| **Multicast Response** | Typically 1 in mDNS | Multicast | Announcement or multicast query response visible to all listeners |

#### Response Examples

**Example 1: Simple A Record Response (Unicast)**
```
Responding to a unicast query for "homeserver.local"

Header:
  TransID: 0x1234 (matches request)
  Flags: 0x8400 (QR=1, AA=1)
  QCount: 1
  ACount: 1

Question Section (Echo):
  QNAME: homeserver.local
  QTYPE: 1 (A record)
  QCLASS: 0x0001 (IN)

Answer Section:
  NAME: homeserver.local  (or 0xc00c pointer)
  TYPE: 1 (A record)
  CLASS: 0x0001 (IN, no cache flush)
  TTL: 120
  RDLEN: 4
  RDATA: 192.168.1.100
```

**Example 2: Multicast Announcement (Unsolicited)**
```
Device announcing its presence on the network

Header:
  TransID: 0x0000
  Flags: 0x8400 (QR=1, AA=1)
  QCount: 0 (no questions)
  ACount: 1

Answer Section:
  NAME: mydevice.local
  TYPE: 1 (A record)
  CLASS: 0x8001 (IN + cache flush bit)
  TTL: 120
  RDLEN: 4
  RDATA: 192.168.1.50

Sent to: 224.0.0.251:5353 (IPv4 multicast)
         [ff02::fb]:5353 (IPv6 multicast)
```

**Example 3: Response with Multiple Records**
```
Response for "homeserver.local" with both A and AAAA

Header:
  TransID: 0x5678
  Flags: 0x8400 (QR=1, AA=1)
  QCount: 1
  ACount: 2

Question Section:
  QNAME: homeserver.local
  QTYPE: 255 (ANY - asking for all records)
  QCLASS: 0x0001 (IN)

Answer Section (1st record):
  NAME: homeserver.local
  TYPE: 1 (A record)
  CLASS: 0x0001 (IN)
  TTL: 120
  RDLEN: 4
  RDATA: 192.168.1.100

Answer Section (2nd record):
  NAME: homeserver.local
  TYPE: 28 (AAAA record)
  CLASS: 0x0001 (IN)
  TTL: 120
  RDLEN: 16
  RDATA: 2001:db8::1
```

---

[↑ back to top](#table-of-contents)

## Resource Record (RR) Types

mDNS supports various resource record types. The most common in local networks are:

### Address Records

#### A Record (Type 1) - IPv4 Address

**Purpose**: Maps a hostname to an IPv4 (32-bit) address.

```
Format:
  NAME: Domain name (e.g., "printer.local")
  TYPE: 1
  CLASS: IN (0x0001)
  TTL: Seconds (typically 120 for mDNS)
  RDLEN: 4
  RDATA: 4 bytes representing IPv4 octet sequence
         [Octet1][Octet2][Octet3][Octet4]

Example: "homeserver.local" → 192.168.1.100
  RDATA: [0xC0][0xA8][0x01][0x64]
```

**mDNS Usage**: Devices announce their IPv4 address for local discovery.

#### AAAA Record (Type 28) - IPv6 Address

**Purpose**: Maps a hostname to an IPv6 (128-bit) address.

```
Format:
  NAME: Domain name (e.g., "printer.local")
  TYPE: 28
  CLASS: IN (0x0001)
  TTL: Seconds (typically 120 for mDNS)
  RDLEN: 16
  RDATA: 16 bytes representing IPv6 address (8 groups of 2 bytes)
         [Bytes0-1][Bytes2-3][Bytes4-5][Bytes6-7]
         [Bytes8-9][Bytes10-11][Bytes12-13][Bytes14-15]

Example: "homeserver.local" → 2001:db8::1
  RDATA: [0x20][0x01][0x0d][0xb8][0x00][0x00][0x00][0x00]
         [0x00][0x00][0x00][0x00][0x00][0x00][0x00][0x01]
```

**mDNS Usage**: Dual-stack devices announce both IPv4 and IPv6 addresses.

### Service Discovery Records

#### SRV Record (Type 33) - Service Location

**Purpose**: Maps a service name to a hostname and port.

```
Format:
  NAME: Service identifier
        (e.g., "_http._tcp.local", "_ssh._tcp.local")
  TYPE: 33
  CLASS: IN (0x0001)
  TTL: Seconds
  RDLEN: Variable (6 + hostname length)
  RDATA: [Priority (2)] [Weight (2)] [Port (2)] [Target (name)]
         
         Priority: Preference (0-65535, lower = higher priority)
         Weight: Relative weight for equal priorities (0-65535)
         Port: TCP or UDP port number (0-65535)
         Target: Hostname to connect to (domain name, compressed)

Example: "_http._tcp.local" → "webserver.local" on port 8080
  RDATA: [0x00][0x00]  <- Priority = 0
         [0x00][0x00]  <- Weight = 0
         [0x1F][0x90]  <- Port = 8080
         [hostname pointer or name]
```

**mDNS Usage**: Services announce their location and port for discovery.

#### TXT Record (Type 16) - Text Attributes

**Purpose**: Stores text-based key-value pairs describing a service.

```
Format:
  NAME: Service name
  TYPE: 16
  CLASS: IN (0x0001)
  TTL: Seconds
  RDLEN: Variable (sum of all string lengths)
  RDATA: Series of length-prefixed strings (DNS strings)
         Each string: [Length (1 byte)][Data (variable)]
         Typical content: "key1=value1", "key2=value2", etc.

Example: Web server service attributes
  String 1: "path=/myapp"        -> [0x0a]path=/myapp
  String 2: "version=1.0"       -> [0x0b]version=1.0
  
Combined RDATA:
  [0x0a]path=/myapp[0x0b]version=1.0
  Total RDLEN: 1 + 10 + 1 + 11 = 23 bytes
```

**mDNS Usage**: Services provide metadata via TXT records (URLs, versions, capabilities).

### Other Common Records

#### CNAME Record (Type 5) - Canonical Name

**Purpose**: Creates an alias from one hostname to another.

```
Format:
  NAME: Alias name
  TYPE: 5
  CLASS: IN (0x0001)
  TTL: Seconds
  RDLEN: Variable (name length)
  RDATA: Target hostname (domain name, compressed)

Example: "www.local" → "webserver.local"
  NAME: www.local
  RDATA: webserver.local (or compressed pointer)
```

#### MX Record (Type 15) - Mail Exchange

**Purpose**: Specifies mail server for a domain (rarely used in mDNS).

```
Format:
  NAME: Domain name
  TYPE: 15
  CLASS: IN (0x0001)
  TTL: Seconds
  RDLEN: Variable (2 + target name length)
  RDATA: [Preference (2)] [Exchange (name)]
         
         Preference: Priority (0-65535, lower = preferred)
         Exchange: Mail server hostname
```

#### NS Record (Type 2) - Nameserver

**Purpose**: Specifies authoritative nameservers (rarely used in mDNS).

```
Format:
  NAME: Domain name
  TYPE: 2
  CLASS: IN (0x0001)
  TTL: Seconds
  RDLEN: Variable (nameserver name length)
  RDATA: Nameserver hostname (domain name)
```

---

[↑ back to top](#table-of-contents)

## Query and Response Scenarios

### Scenario 1: Simple Address Lookup

**Device A queries**: "What is the IPv4 address of printer.local?"

| Step | Direction | Message Type | Content |
|------|-----------|--------------|---------|
| 1 | A → Multicast | Query | Question: printer.local (A record) |
| 2 | Printer → A | Response | Answer: printer.local = 192.168.1.50 (AA=1) |

### Scenario 2: Service Discovery

**Device A queries**: "What web servers are available on this network?"

| Step | Direction | Message Type | Content |
|------|-----------|--------------|---------|
| 1 | A → Multicast | Query | Question: _http._tcp.local (SRV record) |
| 2 | Server1 → A | Response | Answer: _http._tcp.local → server1.local:8080 (AA=1) |
| 3 | Server2 → A | Response | Answer: _http._tcp.local → server2.local:8080 (AA=1) |

### Scenario 3: Conflict Resolution

**Device A claims**: "My name is device.local = 192.168.1.10"

| Step | Direction | Message Type | Content |
|------|-----------|--------------|---------|
| 1 | A → Multicast | Announcement | Answer: device.local = 192.168.1.10 (AA=1) |
| 2 | B → Multicast | Conflict | Answer: device.local = 192.168.1.11 (AA=1) [claiming same name!] |
| 3 | A → Multicast | Resolution | Answer: deviceA.local = 192.168.1.10 (AA=1) [renames itself] |

### Scenario 4: Known-Answer Suppression

**Device A asks** (with Known-Answer List): "What is printer.local? I know it's 192.168.1.50"

| Step | Direction | Message Type | Content |
|------|-----------|--------------|---------|
| 1 | A → Multicast | Query | Question: printer.local (A record) + Known-Answer: printer.local=192.168.1.50 |
| 2 | Printer | Suppresses | [Does NOT respond, as KnownAnswer matches] |

---

[↑ back to top](#table-of-contents)

## Response Rules (RFC 6762)

### When to Respond

A device responds to a query if:
- ✓ It has authority over the queried name (AA=1)
- ✓ The query asks for a type it has authority for (A, AAAA, SRV, TXT, etc.)
- ✓ The queried class matches (usually IN)

### When to Suppress Response

A device suppresses a response (does NOT answer) if:
- ✓ A Known-Answer in the query exactly matches the device's record
- ✓ The record's data is identical
- ✓ The record's TTL is at least half the original value

### Response Timing

| Scenario | Response Type | Delay |
|----------|---------------|-------|
| Multicast query, no conflicts | Multicast answer | 20-120ms random delay |
| Unicast response requested | Unicast answer | 0-10ms |
| Conflict detected | Multicast reannouncement | 250-1000ms (wait & verify) |
| Duplicate suppression | No response | Entire TTL period |

### Cache Flush Behavior

When a device sends an RR with **Cache Flush bit** (0x8000 in CLASS field):
- All other devices **discard cached copies** of that RR
- Devices treat it as a "fresh" announcement
- Typical TTL: 120 seconds (unchanged by cache flush)

---

[↑ back to top](#table-of-contents)

## Summary Table: Common Record Types

| Type | Code | Size | Purpose | mDNS Use |
|------|------|------|---------|----------|
| A | 1 | 4 bytes | IPv4 address | ✓ Common |
| NS | 2 | Variable | Nameserver | ✗ Rarely |
| CNAME | 5 | Variable | Alias | ✓ Occasional |
| SOA | 6 | Variable | Authority | ✗ Rarely |
| MX | 15 | Variable | Mail server | ✗ Rarely |
| TXT | 16 | Variable | Text data | ✓ Very common |
| AAAA | 28 | 16 bytes | IPv6 address | ✓ Common |
| SRV | 33 | Variable | Service | ✓ Very common |

---

**← Back to [README](README.md)**  
**← Previous: [Packet Format](packet-format.md)**  
**Next → [A-Record Exchange Guide](a-record-exchange.md)**
