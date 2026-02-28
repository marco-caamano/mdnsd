# A-Record Exchange: Step-by-Step Guide

This document walks through a complete mDNS A-record query and response exchange, showing all the stages from initial query to response receipt.

## Setting the Scene

**Scenario**: A user opens a web browser and tries to visit `homeserver.local`. The browser needs to discover the IPv4 address of the homeserver device on the local network.

**Network Setup**:
- **Querying Device (Client)**: Connected to local network, needs to resolve `homeserver.local`
- **Responding Device (Server)**: Running mDNS responder, has hostname `homeserver.local` with IPv4 address `192.168.1.100`
- **Network**: Local area network (LAN) with multicast support
- **Multicast Address**: `224.0.0.251:5353` (IPv4) or `[ff02::fb]:5353` (IPv6)

---

## Complete Exchange Flow

### Stage 1: Client Preparation

**Time**: T=0ms

The client application (browser, ping, etc.) needs to resolve the hostname.

```
┌─────────────────────────────────────────────────────┐
│ Application Layer (Browser)                         │
│ "Resolve: homeserver.local"                         │
└──────────────┬──────────────────────────────────────┘
               ↓
┌─────────────────────────────────────────────────────┐
│ mDNS Resolver / DNS Library                         │
│ - Check local resolution cache                      │
│ - Not found in cache → must query                   │
└──────────────┬──────────────────────────────────────┘
               ↓
        Construct Query Packet
```

**Client determines**:
- This is a `.local` domain → Use mDNS
- Need IPv4 address → Query type = A record (type 1)
- Use multicast → Send to `224.0.0.251:5353`

### Stage 2: Query Packet Construction

**Time**: T=0ms

The client builds an mDNS query packet with the following structure:

#### Query Packet Layout

```
QUERY PACKET (33 bytes total)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[DNS HEADER - 12 bytes]
  Offset  Field Name               Value       Meaning
  ──────  ──────────────────────   ───────────────────────
  0-1     Transaction ID           0x0000      No echo needed (multicast)
  2-3     Flags                    0x0000      Query (QR=0, all flags=0)
  4-5     Question Count           0x0001      1 question
  6-7     Answer Count             0x0000      0 answers
  8-9     Authority Count          0x0000      0 authority records
  10-11   Additional Count         0x0000      0 additional records

[QUESTION SECTION - 22 bytes]
  Offset  Field Name               Value       Meaning
  ──────  ──────────────────────   ───────────────────────
  12      QNAME Label 1 Length     0x0A        "homeserver" = 10 chars
  13-22   QNAME Label 1 Data       homeserver  The text "homeserver"
  23      QNAME Label 2 Length     0x05        "local" = 5 chars
  24-28   QNAME Label 2 Data       local       The text "local"
  29      QNAME Root Label         0x00        End of name
  30-31   QTYPE                    0x0001      A record (IPv4)
  32-33   QCLASS                   0x0001      IN (Internet, multicast OK)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Total: 34 bytes
```

#### Raw Hex Representation

```
00 00               <- TransID = 0x0000
00 00               <- Flags = 0x0000 (Query)
00 01               <- QCount = 1
00 00               <- ACount = 0
00 00               <- Authority Count = 0
00 00               <- Additional Count = 0

0A                  <- QNAME: "homeserver" length = 10
68 6f 6d 65 73 65 72 76 65 72  <- "homeserver" ASCII
05                  <- "local" length = 5
6c 6f 63 61 6c      <- "local" ASCII
00                  <- Root label (end)

00 01               <- QTYPE = A record
00 01               <- QCLASS = IN (multicast response OK)
```

#### Human-Readable Interpretation

```
DNS Header:
  This is a QUERY (not a response)
  No special flags are set
  Contains 1 question, 0 answers

Question:
  Name: homeserver.local
  Type: A (IPv4 address)
  Class: IN (Internet, public multicast group)
```

---

### Stage 3: Query Transmission

**Time**: T=0ms

The client sends the query packet to the multicast group.

```
┌──────────────────────────────────────────────────────┐
│ Client Socket Layer                                  │
├──────────────────────────────────────────────────────┤
│ Socket Created:                                      │
│   - AF_INET (IPv4) or AF_INET6 (IPv6)               │
│   - SOCK_DGRAM (UDP)                                │
│   - Port: any (ephemeral, e.g., 54321)              │
│                                                      │
│ Packet Routing:                                      │
│   Destination: 224.0.0.251:5353 (mDNS multicast)    │
│   TTL/Hop Limit: 255 (local network only)           │
│   Interface: Specified local network interface      │
└──────────────────────────────────────────────────────┘
       ↓ (UDP packet sent)
┌──────────────────────────────────────────────────────┐
│ Network Layer                                        │
│ - Packet encapsulated in IP header                  │
│ - Set destination: 224.0.0.251                      │
│ - TTL set to 255 (won't traverse routers)           │
└──────────────────────────────────────────────────────┘
       ↓ (Ethernet frame sent)
┌──────────────────────────────────────────────────────┐
│ Link Layer                                           │
│ - Frame encapsulated in Ethernet header             │
│ - Destination MAC: Multicast MAC (01:00:5E:00:00:FB)│
│ - Broadcast to all devices on local LAN            │
└──────────────────────────────────────────────────────┘
```

**Packet Journey**:

```
┌──────────┐                                    ┌──────────┐
│ Client   │────────────────────────────────────│ Homeserver│
│ Device A │ Query: "homeserver.local (A)?"    │ Responder │
│          │ via 224.0.0.251:5353               │          │
│          │                                    │          │
└──────────┘                                    └──────────┘
   ↓                                              ↑
   └──────→ All other devices on network ←──────┘
            (receive multicast packet)
```

---

### Stage 4: Network Delivery

**Time**: T=0-5ms (approximately)

All devices on the local network receive the multicast packet in their UDP port 5353 socket.

```
Device A (Querier)   <- Receives its own query (loopback)
Device B (Printer)   <- Receives query, doesn't match ("printer.local" ≠ "homeserver.local")
Device C (Router)    <- Receives query, doesn't match
Device D (Homeserver)← Receives query, MATCHES! ("homeserver.local" = own hostname)
Device E (Phone)     <- Receives query, doesn't match
...
```

---

### Stage 5: Server-Side Processing

**Time**: T=5-10ms

The homeserver device processes the received query packet:

#### Step 5.1: Packet Reception and Parsing

```
┌──────────────────────────────────────────────────────┐
│ Homeserver Responder (Device D)                      │
├──────────────────────────────────────────────────────┤
│ 1. recvfrom(socket, buffer, MAX_SIZE, src_addr)    │
│    └─ Receives 33-byte query packet                │
│                                                      │
│ 2. Check DNS Header:                              │
│    - QR flag = 0 (it's a query, not a response) ✓  │
│    - Opcode = 0 (standard query) ✓                 │
│    - QCount = 1 (one question) ✓                   │
│                                                      │
│ 3. Parse Question Section:                          │
│    - Extract name: "homeserver.local"              │
│    - Extract type: 1 (A record)                    │
│    - Extract class: 1 (IN/Internet)                │
│                                                      │
│ 4. Decision Point:                                  │
│    Do I have authority over "homeserver.local"?    │
│    YES ✓ (It's my hostname!)                       │
│                                                      │
│ 5. Decision Point:                                  │
│    Do I have an A record for this name?            │
│    YES ✓ (IPv4 address: 192.168.1.100)             │
└──────────────────────────────────────────────────────┘
```

#### Step 5.2: Authority Verification

```
Server's Knowledge Base:
┌──────────────────────────────────────────────────────┐
│ Hostname Database / Host File                        │
├──────────────────────────────────────────────────────┤
│ Name:        homeserver.local                        │
│ IPv4:        192.168.1.100 ✓ MATCH                  │
│ IPv6:        2001:db8::1 (not requested)            │
│ Authority:   YES (this is my hostname)              │
│ TTL:         120 seconds (standard for mDNS)        │
└──────────────────────────────────────────────────────┘
```

---

### Stage 6: Response Construction

**Time**: T=10-15ms

The homeserver device constructs an mDNS response packet.

#### Response Packet Layout

```
RESPONSE PACKET (41 bytes total)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[DNS HEADER - 12 bytes]
  Offset  Field Name               Value       Meaning
  ──────  ──────────────────────   ───────────────────────
  0-1     Transaction ID           0x0000      Same as query (match)
  2-3     Flags                    0x8400      Response (QR=1, AA=1)
  4-5     Question Count           0x0001      1 question (echo)
  6-7     Answer Count             0x0001      1 answer
  8-9     Authority Count          0x0000      0 authority
  10-11   Additional Count         0x0000      0 additional

[QUESTION SECTION - 22 bytes] (echoed from query)
  Offset  Field Name               Value       Meaning
  ──────  ──────────────────────   ───────────────────────
  12      QNAME Label 1 Length     0x0A        "homeserver"
  13-22   QNAME Label 1 Data       homeserver  Text
  23      QNAME Label 2 Length     0x05        "local"
  24-28   QNAME Label 2 Data       local       Text
  29      QNAME Root               0x00        End
  30-31   QTYPE (echo)             0x0001      A record
  32-33   QCLASS (echo)            0x0001      IN

[ANSWER SECTION - 16 bytes]
  Offset  Field Name               Value       Meaning
  ──────  ──────────────────────   ───────────────────────
  34-35   NAME (compressed)        0xC00C      Pointer to offset 12
                                               (reuse question name)
  36-37   TYPE                     0x0001      A record
  38-39   CLASS                    0x0001      IN (no cache flush)
  40-43   TTL                      0x00000078  120 seconds
  44-45   RDLEN                    0x0004      4 bytes of data
  46-49   RDATA                    0xC0A80164  192.168.1.100
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Total: 50 bytes
```

**Name Compression Explanation**:
```
In the query section (bytes 12-29), the server defined the name:
  12:  0x0A "homeserver" 0x05 "local" 0x00

In the answer section, instead of repeating "homeserver.local",
the server uses a pointer: 0xC00C
  0xC0 (11000000 binary) = "this is a pointer"
  0x0C (00001100 binary) = offset 12

This saves 16 bytes for each answer using the same name (18-byte name replaced by a 2-byte pointer)!
```

#### Raw Hex Representation

```
00 00               <- TransID = 0x0000 (matches query)
84 00               <- Flags = 0x8400 (Response, Authoritative)
00 01               <- QCount = 1
00 01               <- ACount = 1
00 00               <- Authority Count = 0
00 00               <- Additional Count = 0

0A                  <- QNAME Label 1 length = 10
68 6f 6d 65 73 65 72 76 65 72  <- "homeserver"
05                  <- QNAME Label 2 length = 5
6c 6f 63 61 6c      <- "local"
00                  <- Root (end)

00 01               <- QTYPE = A (echo)
00 01               <- QCLASS = IN (echo)

c0 0c               <- NAME = pointer to offset 12 (compressed!)
00 01               <- TYPE = A record
00 01               <- CLASS = IN
00 00 00 78         <- TTL = 120 seconds
00 04               <- RDLEN = 4 bytes
c0 a8 01 64         <- RDATA = 192.168.1.100
                       (192 = 0xC0, 168 = 0xA8, 1 = 0x01, 100 = 0x64)
```

#### Human-Readable Interpretation

```
DNS Header:
  This is a RESPONSE (QR=1)
  This device is AUTHORITATIVE (AA=1)
  Contains 1 question (echo) and 1 answer

Question (Echo):
  Name: homeserver.local
  Type: A (IPv4)
  Class: IN

Answer:
  Name: homeserver.local
  Type: A (IPv4)
  Class: IN
  TTL: 120 seconds
  Data: 192.168.1.100
```

---

### Stage 7: Response Transmission

**Time**: T=15-20ms

The homeserver device sends the response back to the querying device.

```
┌──────────────────────────────────────────────────────┐
│ Homeserver Socket Layer                              │
├──────────────────────────────────────────────────────┤
│ Send Decision:                                       │
│ - Query came from multicast                         │
│ - No unicast bit in QCLASS (0x0001, not 0x8001)    │
│ - Use UNICAST response (efficiency)                 │
│                                                      │
│ Destination:                                        │
│ - Send to client's IP address                       │
│ - Client's source port (from query)                 │
│ - NOT to multicast group                           │
└──────────────────────────────────────────────────────┘
       ↓ (UDP packet sent)
┌──────────────────────────────────────────────────────┐
│ Network Routing                                      │
│ - Small packet, direct unicast to client's IP      │
│ - Typical RTT: 1-2ms on LAN                         │
└──────────────────────────────────────────────────────┘
```

**Packet Journey**:

```
┌──────────┐                                    ┌──────────┐
│ Client   │←───────────────────────────────────│ Homeserver
│ Device A │ Response: "homeserver.local        │ Responder
│          │  = 192.168.1.100"                 │
│          │ via unicast                        │
└──────────┘                                    └──────────┘
```

---

### Stage 8: Client Reception and Processing

**Time**: T=20-25ms

The client receives and processes the response packet.

#### Step 8.1: Network Reception

```
Client Socket: Listening on UDP port 54321 (ephemeral)
Receives: 50-byte response from 192.168.1.100:5353
```

#### Step 8.2: Packet Parsing

```
┌──────────────────────────────────────────────────────┐
│ Client mDNS Resolver                                 │
├──────────────────────────────────────────────────────┤
│ 1. Parse DNS Header:                                │
│    - QR = 1 (response) ✓                           │
│    - AA = 1 (authoritative answer) ✓               │
│    - ACount = 1 (one answer)                       │
│                                                      │
│ 2. Cross-check with Query:                          │
│    - TransID matches query ✓                        │
│    - Question matches sent question ✓              │
│                                                      │
│ 3. Extract Answer:                                  │
│    - Name: homeserver.local (from pointer)         │
│    - Type: A (IPv4)                                │
│    - TTL: 120 seconds                              │
│    - Address: 192.168.1.100 ✓ FOUND!              │
│                                                      │
│ 4. Cache Result:                                    │
│    - Store in mDNS cache                           │
│    - Set expiration time: NOW + 120 seconds        │
│                                                      │
│ 5. Notify Application:                              │
│    - "homeserver.local resolved to 192.168.1.100" │
└──────────────────────────────────────────────────────┘
```

---

### Stage 9: Application Delivery

**Time**: T=25ms

The client application receives the resolved address.

```
┌──────────────────────────────────────────────────────┐
│ Browser / Application                                │
├──────────────────────────────────────────────────────┤
│ Name Resolution for "homeserver.local":             │
│ ✓ RESOLVED: 192.168.1.100                          │
│                                                      │
│ Next Steps:                                         │
│ - Initiate TCP connection to 192.168.1.100:80      │
│ - Download web content                             │
└──────────────────────────────────────────────────────┘
```

---

## Complete Timeline

```
Time    Event                                           Duration
────────────────────────────────────────────────────────────────────

0ms     Browser requests: "Resolve homeserver.local"
        ↓
5ms     Client sends query to 224.0.0.251:5353
        └─ 33-byte UDP packet

5ms     All devices receive multicast query
        └─ Network propagation: ~1-2ms

10ms    Homeserver parses query
        ├─ Recognizes own hostname ✓
        └─ Prepares response

15ms    Homeserver initiates response
  └─ Sends 50-byte UDP packet unicast

20ms    Client receives response on UDP 54321
        └─ Network propagation: ~1-2ms

25ms    Client parses response
        ├─ Caches: homeserver.local → 192.168.1.100
        └─ Notifies browser

30ms    Browser has address, initiates TCP connection

┌────────────────────────────────────────────────┐
│ Total: ~25-30ms from query to name resolution │
└────────────────────────────────────────────────┘
```

---

## Key Observations

### 1. Multicast Efficiency
- Single query reaches **all devices** on network
- Responder sends **unicast reply** (just to querier)
- Balances discovery needs with network load

### 2. Name Compression
- Response reuses question's name with pointer (0xC00C)
- Saves 16 bytes per answer in this example (18-byte name → 2-byte pointer)
- Can support multiple answers efficiently

### 3. Authority Flag (AA=1)
- Server claims authority with AA bit
- Client can trust this is the authoritative answer
- Not a "cached" response from another device

### 4. TTL and Caching
- 120-second TTL = "cache this for up to 2 minutes"
- Same query within 120s can use cached result (no network query!)
- Reduces subsequent query overhead dramatically

### 5. Transaction ID (Zero in Multicast)
- Set to 0x0000 for multicast queries
- Not used for matching (all queries on multicast are answered)
- Would be used for unicast queries (non-zero)

### 6. Response Timing
- Response is sent immediately (or within 20-120ms delay)
- Delay helps prevent network flooding in environments with multiple responders
- No "wait for all responses" — first responder wins

---

## Variations and Edge Cases

### Unicast Response Requested
If the client sends QCLASS = 0x8001 (unicast bit set):
- Server sends **unicast response only** (to querier)
- No other devices hear the response
- Used when client wants quiet resolution (background sync)

### IPv6 Query (AAAA Record)
Replace QTYPE with 0x001C (28 decimal) instead of 0x0001:
- RDATA would be 16 bytes (IPv6 address) instead of 4
- Response would be larger (56 bytes instead of 49)
- Process identical otherwise

### No Response Scenario
If homeserver was offline when query arrived:
- Device never sends response
- Client waits ~1-3 seconds, then retries (protocol depends)
- Retry up to 3-4 times, then fail with "Cannot resolve name"

---

## Summary

This complete A-record exchange demonstrates:
✓ Query construction and multicast transmission  
✓ Server-side parsing and authority verification  
✓ Efficient response construction with name compression  
✓ Quick caching and application notification  
✓ Typical resolution time: **~25-30ms** on local network  

---

**← Back to [README](README.md)**  
**← Previous: [Message Types](message-types.md)**  
**Next → [Multicast Groups](multicast-groups.md)**
