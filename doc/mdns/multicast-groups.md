# mDNS Multicast Groups and Network Configuration

This document describes the multicast addresses used by mDNS, how devices join multicast groups, and the networking considerations for mDNS deployment.

## Table of Contents

- [Overview](#overview)
- [IPv4 Multicast Group](#ipv4-multicast-group)
  - [Address](#address)
  - [Binary Representation](#binary-representation)
  - [Characteristics](#characteristics)
  - [Derived Ethernet MAC Address](#derived-ethernet-mac-address)
- [IPv6 Multicast Group](#ipv6-multicast-group)
  - [Address](#address-1)
  - [Binary Representation](#binary-representation-1)
  - [Characteristics](#characteristics-1)
  - [Derived Ethernet MAC Address](#derived-ethernet-mac-address-1)
  - [Scope Levels (Reference)](#scope-levels-reference)
- [Multicast Group Membership](#multicast-group-membership)
  - [Joining the Group (IPv4)](#joining-the-group-ipv4)
  - [Joining the Group (IPv6)](#joining-the-group-ipv6)
  - [Leaving the Group](#leaving-the-group)
- [Network Interface Binding](#network-interface-binding)
  - [UDP Socket Creation and Binding](#udp-socket-creation-and-binding)
- [Multicast TTL and Hop Limit](#multicast-ttl-and-hop-limit)
  - [Purpose](#purpose)
  - [IPv4: IP_MULTICAST_TTL](#ipv4-ip_multicast_ttl)
  - [IPv6: IPV6_MULTICAST_HOPLIMIT](#ipv6-ipv6_multicast_hoplimit)
- [Network Configuration Examples](#network-configuration-examples)
  - [Example 1: Single Interface mDNS Responder](#example-1-single-interface-mdns-responder)
  - [Example 2: Dual Stack (IPv4 + IPv6)](#example-2-dual-stack-ipv4--ipv6)
  - [Example 3: Multi-Interface Router](#example-3-multi-interface-router)
- [Route and Forwarding](#route-and-forwarding)
  - [Multicast Routing Table](#multicast-routing-table)
- [Troubleshooting Multicast Connectivity](#troubleshooting-multicast-connectivity)
  - [Check: Is Multicast Enabled on Interface?](#check-is-multicast-enabled-on-interface)
  - [Check: Can We Bind to Port 5353?](#check-can-we-bind-to-port-5353)
  - [Check: Firewall Rules](#check-firewall-rules)
  - [Debug: tcpdump Capture](#debug-tcpdump-capture)
- [Special Cases and Considerations](#special-cases-and-considerations)
  - [Link-Local Scope (IPv6)](#link-local-scope-ipv6)
  - [Loopback Interface](#loopback-interface)
  - [Virtual Machines and Containers](#virtual-machines-and-containers)
- [Summary](#summary)

## Overview

mDNS operates entirely using **multicast communication**. All devices on a local network listen to a specific multicast address on UDP port 5353, allowing them to participate in the mDNS protocol without a central server.

---

## IPv4 Multicast Group

### Address

```
IP Address:     224.0.0.251
UDP Port:       5353
Combined:       224.0.0.251:5353
Format:         224.0.0.251/32 (single address, no range)
```

### Binary Representation

```
Decimal:     224 . 0 . 0 . 251
Hex:         0xE0 0x00 0x00 0xFB
Binary:      11100000 . 00000000 . 00000000 . 11111011
             ││└─ IP Class D (multicast) indicator
             └┴─ Reserved for all hosts on this subnet
```

### Characteristics

| Property | Value | Explanation |
|----------|-------|-------------|
| **Class** | D | Multicast address (224.0.0.0 - 239.255.255.255) |
| **Scope** | Link-local | All devices on the local network segment |
| **Router Forwarding** | NO | Routers discard packets to 224.x.x.x |
| **TTL Default** | 255 | Source sets TTL=255; routers decrement to 0 |
| **Ethernet MAC** | 01:00:5E:00:00:FB | Derived from IP address |

### Derived Ethernet MAC Address

Multicast IPv4 addresses map to Ethernet MAC addresses:

```
IPv4 Multicast: 224.0.0.251
                 ││   ││   ││ ││
Ethernet MAC:   01:00:5E:00:00:FB
                 └│ └─ Always 01:00:5E for IPv4 multicast
                      └─ Last 3 octets of IP (00:00:FB)
```

**Formula**:
- First 3 bytes: `01:00:5E` (constant for IPv4 multicast)
- Last 3 bytes: Take the lower 23 bits of the IP address
  - IP: 224.0.0.251
  - Lower 23 bits of last 3 octets: 0x00:0x00:0xFB
  - Result: `01:00:5E:00:00:FB`

---

[↑ back to top](#table-of-contents)

## IPv6 Multicast Group

### Address

```
IP Address:     ff02::fb
Prefix:         ff02::/64
UDP Port:       5353
Combined:       [ff02::fb]:5353
Full Format:    [ff02:0000:0000:0000:0000:0000:0000:00fb]:5353
```

### Binary Representation

```
Notation:    ff02 : : fb
Expanded:    ff02:0000:0000:0000:0000:0000:0000:00fb

Binary (first 16 bits):
            11111111 00000010 (0xff02)
            └─ ff = IPv6 multicast indicator
               02 = Link-local scope
```

### Characteristics

| Property | Value | Explanation |
|----------|-------|-------------|
| **Scope** | ff02 (Link-local) | All devices on the same link (similar to IPv4 LAN segment) |
| **Router Forwarding** | NO | Router removes packets; not forwarded beyond link |
| **Hop Limit** | 255 | Source sets hop limit = 255 (maximum) |
| **Ethernet MAC** | 33:33:00:00:00:FB | Derived from IPv6 address |
| **Interface Requirement** | Required | Sender must specify interface (link-local requires context) |

### Derived Ethernet MAC Address

IPv6 multicast addresses map to Ethernet MAC addresses:

```
IPv6 Multicast: ff02::fb
                 ────       ────
Ethernet MAC:   33:33:00:00:00:fb
                 └─ Always 33:33 for IPv6 multicast
                      └─ Last 4 octets of IPv6 (00:00:00:fb)
```

### Scope Levels (Reference)

| Scope Prefix | Value | Scope Name | Meaning |
|--------------|-------|------------|---------|
| ff01 | 1 | Interface-local | This device only |
| ff02 | 2 | Link-local | All devices on this network link |
| ff05 | 5 | Site-local | Organization-level (deprecated) |
| ff0e | 14 | Global | Internet-wide (rarely used) |

**mDNS uses ff02 (link-local)**, ensuring packets don't leave the local network.

---

[↑ back to top](#table-of-contents)

## Multicast Group Membership

### Joining the Group (IPv4)

Before a device can receive mDNS queries/responses, it must **join the multicast group**.

#### System Call (POSIX)

```c
// Join IPv4 multicast group 224.0.0.251
struct ip_mreq mreq;
mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
mreq.imr_interface.s_addr = inet_addr("192.168.1.100");  // Local interface

setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
```

**Parameters**:
- `imr_multiaddr`: The multicast group to join (224.0.0.251)
- `imr_interface`: The local interface to listen on (e.g., eth0, wlan0)

#### What Happens Internally

```
Step 1: System checks interface availability
        └─ "Is eth0 connected to a network?"

Step 2: Generate IGMP Membership Report
        ├─ Type: Membership Report (IGMP Type 2 or 3)
        ├─ Multicast Address: 224.0.0.251
        └─ Send to local network segment

Step 3: Update network switch forwarding table
        ├─ Entry: "Port X is interested in 224.0.0.251"
        └─ Switch forwards packets to that port

Step 4: Update local routing table
        ├─ Kernel adds route: 224.0.0.251/32 → IF:eth0
        └─ UDP/5353 packets bound to this multicast group
```

### Joining the Group (IPv6)

```c
// Join IPv6 multicast group ff02::fb
struct ipv6_mreq mreq6;
inet_pton(AF_INET6, "ff02::fb", &mreq6.ipv6mr_multiaddr);
mreq6.ipv6mr_interface = if_nametoindex("eth0");  // Interface index

setsockopt(socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6));
```

**Key Difference from IPv4**: IPv6 requires **interface index** (not IP address), because link-local addresses require interface context.

### Leaving the Group

```c
// Drop membership (IPv4)
setsockopt(socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));

// Drop membership (IPv6)
setsockopt(socket, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, &mreq6, sizeof(mreq6));
```

---

[↑ back to top](#table-of-contents)

## Network Interface Binding

### UDP Socket Creation and Binding

#### Step 1: Create UDP Socket

```c
// IPv4
int sock = socket(AF_INET, SOCK_DGRAM, 0);

// IPv6
int sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
```

#### Step 2: Enable Socket Options

```c
// Allow reuse of port if in TIME_WAIT state
int reuse = 1;
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

// IPv6 only: disable IPv4 access
int v6only = 1;
setsockopt(sock6, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
```

#### Step 3: Bind to Port

```c
// IPv4
struct sockaddr_in addr;
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all interfaces
addr.sin_port = htons(5353);               // mDNS port

bind(sock, (struct sockaddr*)&addr, sizeof(addr));

// IPv6
struct sockaddr_in6 addr6;
addr6.sin6_family = AF_INET6;
addr6.sin6_addr = in6addr_any;             // Listen on all interfaces [::1]
addr6.sin6_port = htons(5353);             // mDNS port

bind(sock6, (struct sockaddr*)&addr6, sizeof(addr6));
```

#### Step 4: Specify Network Interface

```c
// Set sending interface (IPv4)
struct ip_mreq mreq;
mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
mreq.imr_interface.s_addr = inet_addr("192.168.1.1");  // eth0 IP

setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &mreq.imr_interface, 
           sizeof(mreq.imr_interface));

// Set sending interface (IPv6)
unsigned int ifindex = if_nametoindex("eth0");
setsockopt(sock6, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex));
```

---

[↑ back to top](#table-of-contents)

## Multicast TTL and Hop Limit

### Purpose

Prevent mDNS packets from leaving the local network (link-local scope).

### IPv4: IP_MULTICAST_TTL

```c
// Set to 255 (maximum, ensures local network coverage)
int ttl = 255;
setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
```

**RFC 6762 Requirement**: TTL must be set to 255 on outgoing packets.

**Router Behavior**:
```
Packet Hop Limit: 255 (from source)
                  ↓ passes through switch (no decrement)
                  ↓ reaches edge router
                  ↗─ TTL=255 → Don't forward (reserved for link-local)
                  └─ TTL<255 → Forward to next network (normal forwarding)
```

### IPv6: IPV6_MULTICAST_HOPS

```c
// Set to 255 (IPv6 equivalent of TTL)
int hops = 255;
setsockopt(sock6, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops));
```

**Received Packet Processing**:
- Hop limit in received packet typically 255
- Local system accepts (on link-local group ff02::/10)
- Packets with hop limit < 1 are dropped

---

[↑ back to top](#table-of-contents)

## Network Configuration Examples

### Example 1: Single Interface mDNS Responder

**Scenario**: Device with one network interface (e.g., home server)

```
Device: homeserver
Interface: eth0 (192.168.1.100)
Multicast: 224.0.0.251:5353

Configuration:
┌─────────────────────────────────────────────────┐
│ Socket: AF_INET, SOCK_DGRAM                    │
│ Bind: 0.0.0.0:5353                            │
│ IP_ADD_MEMBERSHIP: 224.0.0.251, 192.168.1.100 │
│ IP_MULTICAST_IF: 192.168.1.100 (eth0)         │
│ IP_MULTICAST_TTL: 255                         │
├─────────────────────────────────────────────────┤
│ Result:                                         │
│ ✓ Listens on all interfaces (wildcard bind)   │
│ ✓ Joins multicast on eth0 only                │
│ ✓ Sends responses from eth0                   │
│ ✓ TTL=255 (link-local only)                   │
└─────────────────────────────────────────────────┘
```

### Example 2: Dual-Stack (IPv4 + IPv6) Responder

**Scenario**: Device supporting both IPv4 and IPv6

```
Device: homeserver
Interface: eth0 (192.168.1.100, 2001:db8::1)
Multicast: 224.0.0.251:5353 (IPv4) + ff02::fb:5353 (IPv6)

Configuration:
┌────────────────────────────────────────────────────────┐
│ Socket 1: AF_INET, SOCK_DGRAM                         │
│   Bind: 0.0.0.0:5353                                 │
│   IP_ADD_MEMBERSHIP: 224.0.0.251, 192.168.1.100      │
│   IP_MULTICAST_IF: 192.168.1.100                      │
│                                                        │
│ Socket 2: AF_INET6, SOCK_DGRAM                        │
│   Bind: [::]:5353                                     │
│   IPV6_ADD_MEMBERSHIP: ff02::fb, ifindex(eth0)       │
│   IPV6_MULTICAST_IF: ifindex(eth0)                    │
│                                                        │
│ Both: MULTICAST_TTL/HOPS = 255                        │
├────────────────────────────────────────────────────────┤
│ Result:                                                │
│ ✓ Listens on IPv4 multicast 224.0.0.251:5353        │
│ ✓ Listens on IPv6 multicast ff02::fb:5353           │
│ ✓ Can respond to A records (IPv4)                     │
│ ✓ Can respond to AAAA records (IPv6)                  │
│ ✓ Full dual-stack compatibility                       │
└────────────────────────────────────────────────────────┘
```

### Example 3: Multi-Interface Router/Gateway

**Scenario**: Device with multiple network interfaces

```
Device: router
Interfaces: eth0 (192.168.1.1), wlan0 (192.168.2.1)

Configuration Challenge:
  Multicast scoped to link-local only
  → Each interface sees query on its own if joined
  → Must send response on interface query arrived on

Typical Implementation:
┌──────────────────────────────────┐      ┌──────────────────────────────────┐
│ eth0 (192.168.1.0/24)            │      │ wlan0 (192.168.2.0/24)           │
├──────────────────────────────────┤      ├──────────────────────────────────┤
│ Socket bound to eth0:            │      │ Socket bound to wlan0:           │
│  - addmembership 224.0.0.251     │      │  - addmembership 224.0.0.251    │
│  - IP_MULTICAST_IF: 192.168.1.1  │      │  - IP_MULTICAST_IF: 192.168.2.1 │
│ │                                │      │ │                                │
│ Receives queries from eth0 only  │      │ Receives queries from wlan0 only│
│ Responds back to eth0 sender     │      │ Responds back to wlan0 sender   │
└──────────────────────────────────┘      └──────────────────────────────────┘

Result: Each interface isolated (expected for link-local scope)
        No cross-interface discovery (Design per RFC 6762)
```

---

[↑ back to top](#table-of-contents)

## Route and Forwarding

### Multicast Routing Table

```bash
# View multicast routes
route -n
netstat -rn

# Example output:
Destination     Gateway       Genmask         Iface
224.0.0.251     0.0.0.0       255.255.255.255 eth0
ff02::fb        ::            ffff:ffff::     eth0 (scope: link)
```

### Router Behavior on Multicast Packets

```
Incoming Query: 224.0.0.251:5353 on Router Port X
         ↓
Router checks routing table:
  ✓ Found: "224.0.0.251/32 → eth0"
  ├─ TTL = 255 (source set max)
  ├─ Destination = multicast address
  └─ Decision: DO NOT FORWARD (reserved, link-local only)
             
Result: Packet stays on eth0 segment
        Never reaches other networks
```

---

[↑ back to top](#table-of-contents)

## Troubleshooting Multicast Connectivity

### Check: Is Multicast Enabled on Interface?

```bash
# Linux
ip link show eth0
# Look for: MULTICAST (indicates support)

# macOS
ifconfig en0 | grep MULTICAST
```

### Check: Join Status

```bash
# View multicast group memberships
ip maddr show eth0

# Example output:
inet 224.0.0.251   <- Successfully joined IPv4 group
inet6 ff02::fb     <- Successfully joined IPv6 group
```

### Check: Listen on Port 5353

```bash
# Linux
netstat -ulnp | grep 5353
ss -ulnp | grep 5353

# macOS
netstat -an | grep 5353
lsof -i :5353
```

### Check: Network Connectivity

```bash
# Send test query
nmap -p 5353 -sU -P0 192.168.1.0/24

# Use mdns-resolver
dns-sd -B _services._dns-sd._udp local
```

---

[↑ back to top](#table-of-contents)

## Special Cases and Considerations

### Link-Local Scope (IPv6)

IPv6 link-local addresses require interface specification:

```c
// WRONG: IPv6 requires interface context
inet_pton(AF_INET6, "ff02::fb", &addr);
// Doesn't know which eth0, wlan0, etc.

// CORRECT: Specify interface by index
unsigned int ifindex = if_nametoindex("eth0");
struct ipv6_mreq mreq6;
mreq6.ipv6mr_interface = ifindex;  // Required for link-local!
```

### VPN and Tunnels

mDNS does **NOT** traverse VPN tunnels by default:
- VPN creates separate network interface
- Multicast scope = link-local only
- VPN interface doesn't see mDNS from main network
- Exception: Some VPN software bridges interfaces

### Firewall Considerations

mDNS typically requires firewall exceptions:

```
UDP 5353/mdns: ALLOW (outbound & inbound)
224.0.0.251: ALLOW (IPv4 multicast address)
ff02::fb: ALLOW (IPv6 multicast address)
```

### Container Environments (Docker)

Multicast in containers requires special setup:

```bash
# Docker container multicast support
docker run --net host <image>  # Use host network

# OR bridge network with multicast
docker run --net bridge \
  --cap-add NET_ADMIN \
  <image>
```

---

[↑ back to top](#table-of-contents)

## Summary

**IPv4 mDNS**:
- Group: `224.0.0.251:5353`
- Scope: Link-local (no router forwarding)
- TTL: 255 (enforces local scope)
- Join: IP_ADD_MEMBERSHIP
- MAC: 01:00:5E:00:00:FB

**IPv6 mDNS**:
- Group: `[ff02::fb]:5353`
- Scope: Link-local (ff02::/10)
- Hop Limit: 255 (enforces local scope)
- Join: IPV6_ADD_MEMBERSHIP with interface index
- MAC: 33:33:00:00:00:FB

**Key Points**:
✓ Multicast keeps traffic localized to one network segment  
✓ Each interface must explicitly join the group  
✓ TTL/Hop limit prevents cross-network leakage  
✓ IPv6 requires interface specification (link-local context)  
✓ Routers never forward mDNS packets beyond the local link  

---

**← Back to [README](README.md)**  
**← Previous: [A-Record Exchange Guide](a-record-exchange.md)**
