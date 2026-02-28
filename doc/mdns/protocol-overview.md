# mDNS Protocol Overview

## Table of Contents

- [Introduction](#introduction)
- [History and Motivation](#history-and-motivation)
- [Core Differences from Traditional DNS](#core-differences-from-traditional-dns)
- [mDNS Protocol Fundamentals](#mdns-protocol-fundamentals)
- [Key Operational Behaviors](#key-operational-behaviors)
- [Summary](#summary)

## Introduction

Multicast DNS (mDNS) is a protocol that provides DNS-like name resolution capabilities on local networks without requiring a central DNS server. Defined in **RFC 6762**, mDNS operates by having devices multicast queries to their local network and by having devices listening on the network respond with answers from their local zone.

**Key Characteristic**: mDNS uses the `.local` domain suffix to distinguish local-network hostnames from traditional DNS names.

[↑ back to top](#table-of-contents)

## History and Motivation

Before mDNS, local hostname resolution on networks without a configured DNS server was problematic:
- Users had to maintain `/etc/hosts` files manually
- No automated service discovery
- No way for new devices to announce themselves

mDNS solves these problems by:
1. Enabling devices to advertise their presence automatically
2. Allowing other devices to query for service locations
3. Requiring zero administrative configuration

[↑ back to top](#table-of-contents)

## Core Differences from Traditional DNS

| Aspect | Traditional DNS | mDNS |
|--------|-----------------|------|
| **Query Transport** | Unicast (single server) | Multicast (all devices) |
| **Infrastructure** | Requires DNS server | No server needed |
| **Domain Scope** | Global or organizational | Link-local (single network) |
| **Domain Suffix** | Various (.com, .org, etc.) | Always `.local` |
| **Configuration** | Manual setup required | Zero-configuration |
| **Response Handling** | Single authoritative server | Multiple devices may respond |
| **Query Port** | UDP port 53 | UDP port 5353 |

[↑ back to top](#table-of-contents)

## mDNS Protocol Fundamentals

### Multicast Communication

mDNS relies on **multicast addresses** to reach all devices on a local network simultaneously:

- **IPv4 Multicast**: `224.0.0.251:5353`
  - Class D address (224.0.0.0/4 range)
  - Scoped to link-local network
  
- **IPv6 Multicast**: `[ff02::fb]:5353`
  - Link-local scope (ff02::/10)
  - ff02::fb is the IPv6 equivalent of IPv4 224.0.0.251

All mDNS queries and certain responses are sent to these multicast addresses, ensuring all listening devices receive the message.

### Local Domain Convention

The `.local` top-level domain (TLD) is **reserved for mDNS use** (RFC 6762, Section 2):
- Not delegated in the root DNS servers
- Used exclusively for link-local name resolution
- Example hostnames: `printer.local`, `fileserver.local`, `homeserver.local`

### Query and Response Mechanism

#### Queries (Questions)

1. A client device sends an mDNS query to the multicast address containing a **question section**
2. The question specifies:
   - Domain name to resolve (e.g., `homeserver.local`)
   - Query type (e.g., A record for IPv4)
   - Query class (typically IN for Internet)

3. Devices listening receive the query and compare it against their authoritative zone

#### Responses (Answers)

1. Devices with authority over the queried name **respond with answer records**
2. Responses typically unicast back to the querying device (for negative queries) or multicast (for positive responses)
3. Response includes:
   - **Authoritative Answer (AA) flag** set to 1, indicating the device is authoritative
   - **Answer section** containing resource records matching the query
   - **TTL (Time-To-Live)** value for cache management

### Known-Answer Suppression

**Purpose**: Reduce redundant answers when multiple devices have the same information cached.

**Mechanism**:
- A querying device includes **previously cached answers** in the question's "Known-Answer List"
- Devices receiving the query suppress their answers if they're already in the Known-Answer List
- Only devices with better/different information respond

**Example**:
- Device A queries for `homeserver.local` (A record)
- Device A already knows the answer from 30 seconds ago, includes it in Known-Answer List
- Device B (the actual homeserver) sees the known answer and **may suppress its response**
- Device B responds only if its answer differs

### Response Timing and Delays

**Query Response Rules** (RFC 6762, Section 6):

| Situation | Response Type | Timing |
|-----------|---------------|--------|
| Query for existing name | Unicast response | Immediate or delay 20-120ms |
| Multicast query | Multicast response | Delay 25-125ms |
| Rapid repeated queries | Multicast aggregation | As answered together |
| Conflict detected | Multicast announcement | After conflict resolution |

**Purpose of Delays**: 
- Prevent network flooding from queries
- Allow responses to be aggregated
- Let devices detect conflicts before responding

### TTL (Time-To-Live) Handling

**Cache Control**:
- Each resource record includes a TTL value (typically in seconds)
- Devices cache records for the duration of the TTL
- When TTL expires, cached record is discarded

**Special TTL Values** (RFC 6762, Section 10):
- **Zero TTL**: Record should not be cached; used for cache invalidation
- **High TTL** (120+ seconds): Stable, unlikely to change
- **Low TTL** (1-20 seconds): Unstable, may change frequently

**Cache Flushing**:
- TTL values in the range 0x8000-0xFFFF indicate cache flush
- When a device sends a record with the high-order bit (0x8000) set in its TTL, other devices **flush their cached copy**
- Used when a device's address changes

### Conflict Resolution

When multiple devices claim authority over the same name:

1. **Conflict Detection**: Device detects another device responding with different information
2. **Announcement Delay**: Device waits (typically 250-1000ms) before responding
3. **Verification**: Device checks if conflict still exists
4. **Resolution**:
   - If local record is more recent, reassert with multicast announcement
   - If remote record is better, accept it or escalate (e.g., rename hostname)

### Resource Record Types

mDNS primarily supports these resource record types:

| Type | Code | Purpose |
|------|------|---------|
| **A** | 1 | IPv4 address (32-bit) |
| **AAAA** | 28 | IPv6 address (128-bit) |
| **SRV** | 33 | Service location (hostname + port) |
| **TXT** | 16 | Text attributes (service metadata) |
| **CNAME** | 5 | Canonical name (alias) |
| **NS** | 2 | Nameserver (rarely used in mDNS) |
| **MX** | 15 | Mail exchange (rarely used in mDNS) |
| **SOA** | 6 | Start of authority (rarely used in mDNS) |

[↑ back to top](#table-of-contents)

## Key Operational Behaviors

### Announcement vs Query

**Unsolicited Announcement** (Proactive):
- Device announces its presence without being queried
- Sent during startup or when identity changes
- Allows other devices to cache the record

**Solicited Response** (Reactive):
- Device responds to explicit queries
- Sent only when directly questioned

### Negative Responses

If a device doesn't have authority for a queried name:
- **No Response**: Simply don't answer (passive approach)
- **NXDOMAIN** (No Such Domain): Some implementations may respond with negative NXDOMAIN to indicate the name doesn't exist (RFC 6762, Section 8)

### Multicast Response vs Unicast Response

**Multicast Response** (to multicast address):
- Used for unsolicited announcements
- Used for responding to multicast queries
- Heard by all devices on network
- Higher network overhead

**Unicast Response** (to querier's source address):
- Used when query came from unicast
- Used to avoid network flooding
- More efficient for repeated queries

[↑ back to top](#table-of-contents)

## Summary

mDNS provides:
✓ Zero-configuration name resolution  
✓ Local service discovery without infrastructure  
✓ Automatic device announcements  
✓ Conflict resolution across network  
✓ Efficient caching with TTL support  

---

**← Back to [README](README.md)**  
**Next → [Packet Format](packet-format.md)**
