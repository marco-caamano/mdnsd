# DNS/mDNS Packet Format

This document describes the complete structure of mDNS packets, which follow the DNS packet format defined in RFC 1035 and adapted for multicast use in RFC 6762.

## Packet Structure Overview

An mDNS packet consists of **five sections** (though not all may be present):

```
┌─────────────────────────────────┐
│   DNS HEADER (12 bytes)         │ Always present
├─────────────────────────────────┤
│   QUESTION SECTION              │ Contains queries
│   (variable length)             │
├─────────────────────────────────┤
│   ANSWER SECTION                │ Contains responses
│   (variable length)             │
├─────────────────────────────────┤
│   AUTHORITY SECTION             │ Nameserver info
│   (variable length)             │
├─────────────────────────────────┤
│   ADDITIONAL SECTION            │ Extra info
│   (variable length)             │
└─────────────────────────────────┘
```

**Maximum Packet Size**: 1500 bytes (limited by standard Ethernet MTU)

---

## Section 1: mDNS Header (DNS Wire Format, 12 bytes)

### Quick Reference Table

| Offset | Field | Size | Purpose |
|--------|-------|------|---------|
| 0-1 | Transaction ID | 2 bytes | Identifies related queries/responses |
| 2-3 | Flags | 2 bytes | QR, Opcode, AA, TC, RD, RA, Z, RCODE |
| 4-5 | Question Count | 2 bytes | Number of entries in Question section |
| 6-7 | Answer Count | 2 bytes | Number of entries in Answer section |
| 8-9 | Authority Count | 2 bytes | Number of entries in Authority section |
| 10-11 | Additional Count | 2 bytes | Number of entries in Additional section |

### Detailed Bit-Field Diagram

```
                    mDNS HEADER (DNS wire format, 12 bytes)

     Byte 0              Byte 1              Byte 2              Byte 3
  0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7
┌───────────────────────────────────┬───────────────────────────────────┐
│      Transaction ID (16 bits)     │         Flags (16 bits)           │
│   (0x0000 to 0xFFFF)              │                                   │
└───────────────────────────────────┴───────────────────────────────────┘

     Byte 4              Byte 5              Byte 6              Byte 7
  0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7
┌───────────────────────────────────┬───────────────────────────────────┐
│   Question Count (16 bits)        │   Answer Count (16 bits)          │
└───────────────────────────────────┴───────────────────────────────────┘

     Byte 8              Byte 9             Byte 10             Byte 11
  0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7   0 1 2 3 4 5 6 7
┌───────────────────────────────────┬───────────────────────────────────┐
│   Authority Count (16 bits)       │   Additional Count (16 bits)      │
└───────────────────────────────────┴───────────────────────────────────┘
```

### Header Fields Explained

#### Transaction ID (2 bytes, Offset 0-1)
- **Purpose**: Correlate queries with their responses
- **Range**: 0x0000 to 0xFFFF (any 16-bit value)
- **In mDNS**: Often set to 0 for multicast queries (no response matching needed)
- **Byte Order**: Network byte order (big-endian)

#### Flags (2 bytes, Offset 2-3)

The flags field is a 16-bit register with individual flag bits:

```
Bit:  0   1   2 3 4   5   6   7   8   9 10 11  12 13 14 15
    ┌───┬───┬─────┬───┬───┬───┬───┬───┬───────┬─────────┐
    │QR │OPC|OPCODE| AA│ TC│ RD│ RA│ Z │ RCODE (4 bits)│
    └───┴───┴─────┴───┴───┴───┴───┴───┴───────┴─────────┘
     0   1  2-5     6   7   8   9  10  11-15
```

**Flag Definitions**:

| Bit | Name | Value | Meaning |
|-----|------|-------|---------|
| 0 | QR (Query/Response) | 0 | Query |
| | | 1 | Response |
| 1-4 | Opcode | 0 | Standard query |
| | | 1 | Inverse query (obsolete) |
| | | 2 | Server status request |
| | | 4 | Notify |
| | | 5 | Update |
| 5 | AA (Authoritative Answer) | 0 | Not authoritative |
| | | 1 | Authoritative (responder has authority) |
| 6 | TC (Truncated) | 0 | Message not truncated |
| | | 1 | Message exceeded 512 bytes (truncated) |
| 7 | RD (Recursion Desired) | 0 | Recursion not desired |
| | | 1 | Recursion desired |
| 8 | RA (Recursion Available) | 0 | Recursion not available |
| | | 1 | Recursion available |
| 9 | Z (Reserved) | 0 | Must be zero |
| 10-15 | RCODE (Response Code) | 0 | NOERROR - No error |
| | | 1 | FORMERR - Format error |
| | | 2 | SERVFAIL - Server failure |
| | | 3 | NXDOMAIN - Non-existent domain |
| | | 4 | NOTIMP - Not implemented |
| | | 5 | REFUSED - Query refused |

**Typical Flag Values**:

- **Query**: 0x0000 (QR=0, all others = 0)
  - Binary: 0000 0000 0000 0000
  
- **Response (Authoritative)**: 0x8400 (QR=1, AA=1)
  - Binary: 1000 0100 0000 0000
  
- **Response (Not Authoritative)**: 0x8000
  - Binary: 1000 0000 0000 0000

#### Question/Answer/Authority/Additional Counts (2 bytes each)
- **Purpose**: Specify how many records exist in each section
- **Range**: 0 to 65535
- **Byte Order**: Network byte order (big-endian)

---

## Section 2: Question Section (Variable Length)

### Quick Reference

| Field | Size |
|-------|------|
| QNAME (Domain Name) | Variable (1-255 bytes) |
| QTYPE (Query Type) | 2 bytes |
| QCLASS (Query Class) | 2 bytes |

### Detailed Structure

```
Question Section Format (repeats for each question):

┌─────────────────────────────────────────────────────┐
│  QNAME (Variable length, compressed format)         │
│  Example: "homeserver.local"                        │
│  Encoded: [10]homeserver[5]local[0]                 │
├─────────────────────────────────────────────────────┤
│  QTYPE (2 bytes)                                    │
│  Example: 0x0001 = A record (IPv4 address)         │
├─────────────────────────────────────────────────────┤
│  QCLASS (2 bytes)                                   │
│  Example: 0x0001 = IN (Internet)                    │
│  Note: High bit (0x8000) = unicast response desired │
└─────────────────────────────────────────────────────┘
```

### QNAME Format (Domain Name Compression)

DNS uses a **label-based compression scheme** to encode domain names:

#### Uncompressed Format (Used in Question Section):

```
Byte:  0      1-10         11   12-16        17
     ┌──┬──────────────┬──┬──────────────┬──┐
  │0A│homeserver    │05│local         │00│
     └──┴──────────────┴──┴──────────────┴──┘
     Length "homeserver" Length "local"   0=END
```

**Encoding Rules**:
- Each label begins with a **length byte** (0x01-0x3F for normal labels)
- Followed by the ASCII characters of that label
- Root label (final label) is a single zero byte (0x00)
- Example: `homeserver.local` → `[0x0A]homeserver[0x05]local[0x00]`

First-octet ranges in DNS names:
- `0x00`: root/end of name
- `0x01-0x3F`: normal label length (1-63 bytes)
- `0x40-0xBF`: reserved/extended label encodings (not normal mDNS labels)
- `0xC0-0xFF`: compression pointer indicator

#### Compressed Format (Used in Answer/Authority/Additional Sections):

For efficiency in responses, **name compression pointers** are used:

```
Pointer Format:
    Bit: 0 1 | 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        ┌───┴───┬──────────────────────────────┐
        │1   1  │   Offset (14-bit value)      │
        └───────┴──────────────────────────────┘
         Must be 1
         
Example: 0xC00C = pointer to offset 12
         Binary: 1100 0000 0000 1100
         Offset = 0x000C = 12 decimal
```

**Pointer Rules**:
- If the first byte of a name label has bits 7-6 set to `11` binary, it's a **pointer**
- The remaining 14 bits specify the **offset** into the packet where the actual name is located
- Pointer offsets are absolute from the **start of the DNS/mDNS message** (byte 0)
- Pointer `0xC00C` points to byte 12, which is in the **Question section** (start of QNAME)

**Example** (Response with compressed Name):
```
Packet byte map:
  0-11   Header
  12-29  Question QNAME = [0x0A]homeserver[0x05]local[0x00]
  30-31  QTYPE
  32-33  QCLASS
  34-35  Answer NAME (compressed)
  
Answer section (compressed):
  [0xC0][0x0C]      <- pointer to absolute offset 12 (Question QNAME start)
  [Type][Class]...

Space saving in this example:
  Uncompressed NAME in answer: 18 bytes ([0x0A]homeserver[0x05]local[0x00])
  Compressed NAME (pointer):     2 bytes ([0xC0][0x0C])
  Saved per answer RR:          16 bytes
```

How the decoder resolves `0xC00C`:
1. Read first NAME octet in answer: `0xC0` (`11xxxxxx` means "pointer")
2. Combine with next octet `0x0C` → pointer value `0xC00C`
3. Mask top two bits (`0x3FFF`) → offset `0x000C` (decimal 12)
4. Jump to byte 12 in the same packet and read labels until `0x00`
5. Resulting NAME is `homeserver.local`

Why this saves space:
- The full name bytes already exist in the Question section.
- Each answer reuses those existing bytes instead of repeating them.
- With one answer, saving is 16 bytes (18-byte name replaced by 2-byte pointer).
- With multiple answers for the same NAME (e.g., A + AAAA + TXT), each additional answer saves another 16 bytes.

Important constraints:
- Pointers reference offsets in the **same DNS/mDNS message** only.
- Pointers usually point backward to already-encoded names/suffixes.
- A pointer can reference a whole name or a suffix (for example, reuse just `local`).
- `0xC0`/`0xC00C` bytes are pointer metadata, not label text.

### QTYPE (Query Type) - 2 bytes

```
           Byte N          Byte N+1
         0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        ┌──────────────────────────────┐
        │   Type Code (16-bit value)   │
        └──────────────────────────────┘
```

**Common Type Codes**:

| Type Name | Code | Purpose |
|-----------|------|---------|
| A | 1 | IPv4 address (32-bit) |
| NS | 2 | Nameserver |
| CNAME | 5 | Canonical name |
| SOA | 6 | Start of Authority |
| MX | 15 | Mail exchange |
| TXT | 16 | Text records |
| AAAA | 28 | IPv6 address (128-bit) |
| SRV | 33 | Service location |
| ANY | 255 | Any type (wildcard query) |

### QCLASS (Query Class) - 2 bytes

```
           Byte N          Byte N+1
         0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        ┌──────────────────────────────┐
        │   Class Code (16-bit value)  │
        │   High bit: 0=Multicast,1=UC │
        └──────────────────────────────┘
```

**Class Values**:

| Class | Value | Meaning |
|-------|-------|---------|
| IN (Internet) | 1 | Internet (always used in mDNS) |
| CH | 3 | Chaos (rarely used) |
| HS | 4 | Hesiod (obsolete) |

**mDNS Special**: High bit (0x8000) indicates **unicast response desired**
- `0x0001` = Multicast response is acceptable
- `0x8001` = Unicast response desired (suppress multicast)

---

## Section 3,4,5: Answer/Authority/Additional Sections (Variable Length)

These sections contain **Resource Records (RRs)** with the same format.

### Quick Reference

| Field | Size | Description |
|-------|------|-------------|
| NAME | Variable | Domain name (compressed format) |
| TYPE | 2 bytes | RR type (A, AAAA, MX, TXT, SRV, etc.) |
| CLASS | 2 bytes | RR class (typically IN=1) |
| TTL | 4 bytes | Time-To-Live in seconds |
| RDLEN | 2 bytes | Length of RDATA field |
| RDATA | Variable | Record-specific data |

### Detailed Structure

```
Resource Record (RR) Format:

┌────────────────────────────────────────────────┐
│  NAME (Variable, compressed)                   │
│  Example: Pointer 0xC00C = "homeserver.local"  │
├────────────────────────────────────────────────┤
│  TYPE (2 bytes)                                │
│  Example: 0x0001 = A record                    │
├────────────────────────────────────────────────┤
│  CLASS (2 bytes)                               │
│  Example: 0x0001 = IN (Internet)               │
│  High bit (0x8000) = Cache flush (mDNS only)  │
├────────────────────────────────────────────────┤
│  TTL (4 bytes, big-endian)                     │
│  Example: 0x00000078 = 120 seconds             │
├────────────────────────────────────────────────┤
│  RDLEN (2 bytes)                               │
│  Example: 0x0004 = 4 bytes of data follow      │
├────────────────────────────────────────────────┤
│  RDATA (Variable, type-specific)               │
│  For A record: 4 bytes (IPv4 address)          │
│  For AAAA record: 16 bytes (IPv6 address)      │
│  For TXT record: length-prefixed strings       │
│  For SRV record: priority, weight, port, name  │
└────────────────────────────────────────────────┘
```

### CLASS Field (With Cache Flush Bit)

```
           Byte N          Byte N+1
         0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        ┌──────────────────────────────┐
        │CF │     Class (15-bit value) │
        └──────────────────────────────┘
         └─ Bit 15: Cache Flush
```

- **Bit 15 (0x8000)**: Cache Flush flag (mDNS-specific, RFC 6762)
  - `0x0001` = Normal class IN (no cache flush)
  - `0x8001` = Class IN with cache flush requested
  
**Cache Flush Behavior**: When a device sends an RR with the cache flush bit set, other devices should **discard their cached copies** of that RR.

### TTL (Time-To-Live) - 4 bytes

```
     Byte N    Byte N+1  Byte N+2  Byte N+3
  ┌──────────┬──────────┬──────────┬──────────┐
  │ Value (big-endian, 32-bit unsigned)      │
  │ Range: 0 to 2,147,483,647 seconds        │
  │ Typical: 120 seconds for mDNS responses  │
  └──────────┴──────────┴──────────┴──────────┘
```

**Special TTL Values**:
- **0**: Do not cache; used for cache invalidation
- **0x8000 or higher**: Cache flush bit; other devices should flush this record from cache
- **Typical mDNS**: 120 seconds (2 minutes)

### Resource Data (RDATA) - Type-Specific

The RDATA field format depends on the record TYPE:

#### A Record RDATA (IPv4 Address)

```
RDLEN = 4 bytes

    Byte 0          Byte 1          Byte 2          Byte 3
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
┌─────────────────────────────────────────────────────────────────┐
│  Octet 1        │  Octet 2       │  Octet 3       │  Octet 4    │
│  (IPv4 A)       │  (IPv4 B)      │  (IPv4 C)      │  (IPv4 D)   │
└─────────────────────────────────────────────────────────────────┘

Example: 192.168.1.100
         [0xC0][0xA8][0x01][0x64]
```

#### AAAA Record RDATA (IPv6 Address)

```
RDLEN = 16 bytes

    Bytes 0-1    Bytes 2-3    Bytes 4-5    Bytes 6-7
  ┌────────────┬────────────┬────────────┬────────────┐
  │  Octet 0-1 │  Octet 2-3 │  Octet 4-5 │  Octet 6-7 │
  └────────────┴────────────┴────────────┴────────────┘
    Bytes 8-9    Bytes 10-11  Bytes 12-13  Bytes 14-15
  ┌────────────┬────────────┬────────────┬────────────┐
  │  Octet 8-9 │ Octet 10-11│ Octet 12-13│ Octet 14-15│
  └────────────┴────────────┴────────────┴────────────┘

Example: 2001:db8::1
         [0x20][0x01][0x0d][0xb8][0x00][0x00][0x00][0x00]
         [0x00][0x00][0x00][0x00][0x00][0x00][0x00][0x01]
```

#### TXT Record RDATA (Text Attributes)

```
Format: Length-prefixed strings

    Byte 0          Bytes 1-N        Byte N+1       Bytes N+2-M
  ┌──────────┬─────────────────┬──────────┬─────────────────┐
  │ Length 1 │ String Data 1   │ Length 2 │ String Data 2   │
  │ (1 byte) │ (Length 1 bytes)│ (1 byte) │ (Length 2 bytes)│
  └──────────┴─────────────────┴──────────┴─────────────────┘

Each string is prefixed with its length (including "key=value" pairs)
```

---

## Complete Packet Example

### mDNS A Record Query

```
Raw Hex:
00 00  <- Transaction ID
00 00  <- Flags (Query: QR=0)
00 01  <- Question Count = 1
00 00  <- Answer Count = 0
00 00  <- Authority Count = 0
00 00  <- Additional Count = 0
0A 68 6f 6d 65 73 65 72 76 65 72  <- "homeserver" (len=10)
05 6c 6f 63 61 6c  <- "local" (len=5)
00  <- Root (end of name)
00 01  <- QTYPE = A record (1)
00 01  <- QCLASS = IN (1)

Breakdown:
Header:  12 bytes
QNAME:   18 bytes ("homeserver.local")
QTYPE:   2 bytes (A record)
QCLASS:  2 bytes (IN)
---------
Total:   34 bytes
```

### mDNS A Record Response

```
Raw Hex:
00 00  <- Transaction ID (matches query)
84 00  <- Flags (Response: QR=1, AA=1)
00 01  <- Question Count = 1
00 01  <- Answer Count = 1
00 00  <- Authority Count = 0
00 00  <- Additional Count = 0

(Question section - echoed back)
0A 68 6f 6d 65 73 65 72 76 65 72  <- "homeserver"
05 6c 6f 63 61 6c  <- "local"
00  <- Root
00 01  <- QTYPE = A
00 01  <- QCLASS = IN

(Answer section)
c0 0c  <- NAME = pointer to offset 12 (name from question)
00 01  <- TYPE = A record
00 01  <- CLASS = IN (no cache flush)
00 00 00 78  <- TTL = 120 seconds
00 04  <- RDLEN = 4 bytes
c0 a8 01 64  <- RDATA = 192.168.1.100

Breakdown:
Header:  12 bytes
Question: 22 bytes
Answer:  16 bytes (pointer + type + class + TTL + len + data=4)
---------
Total:   50 bytes
```

---

## Key Points

- ✓ All multi-byte values use **network byte order (big-endian)**
- ✓ Domain names use **label-length encoding** and **compression pointers**
- ✓ **Flags field** controls query/response semantics with individual bit flags
- ✓ **TTL values** manage caching; high-order bit may indicate cache flush
- ✓ **mDNS-specific**: Cache flush bit (0x8000) in CLASS field of RRs
- ✓ **Maximum packet size**: 1500 bytes (Ethernet MTU)

---

**← Back to [README](README.md)**  
**← Previous: [Protocol Overview](protocol-overview.md)**  
**Next → [Message Types](message-types.md)**
