# mDNS Protocol Documentation

Welcome to the mDNS Protocol Documentation. This guide provides a comprehensive overview of the Multicast Domain Name System (mDNS) as defined in **RFC 6762**, covering how mDNS works, packet structures, message types, and complete examples of query/response exchanges.

## What is mDNS?

Multicast DNS (mDNS) allows devices to perform DNS-like name resolution on local networks without requiring any traditional DNS infrastructure. It enables zero-configuration service discovery and hostname resolution, making it ideal for local area networks where a centralized DNS server may not be available.

## Documentation Structure

This documentation is organized into focused topics for easy navigation:

| Document | Purpose |
|----------|---------|
| [Protocol Overview](protocol-overview.md) | Core mDNS concepts, multicast fundamentals, and key differences from traditional DNS |
| [Packet Format](packet-format.md) | Complete DNS/mDNS packet structure with detailed bit-field diagrams and quick reference tables |
| [Message Types](message-types.md) | Query and response types, resource record types, and flag meanings |
| [A-Record Exchange Guide](a-record-exchange.md) | Step-by-step walkthrough of a complete mDNS query/response for an A record |
| [Multicast Groups](multicast-groups.md) | IPv4 and IPv6 multicast addressing, group membership, and interface binding |

## Prerequisites

This documentation assumes basic familiarity with:
- DNS concepts (domain names, resource records, queries, responses)
- Network protocols (UDP, IP, multicast)
- Hexadecimal notation and bit-level data representations

## Quick Navigation

**New to mDNS?**
1. Start with [Protocol Overview](protocol-overview.md)
2. Review [Message Types](message-types.md)
3. Walk through [A-Record Exchange Guide](a-record-exchange.md)

**Understanding Packet Structure?**
1. Read [Packet Format](packet-format.md) quick reference tables first
2. Review detailed bit-field diagrams for deeper understanding

**Network Configuration?**
1. See [Multicast Groups](multicast-groups.md) for addresses and interface setup

## Key Concepts at a Glance

- **Local Domain**: Devices use the `.local` suffix for mDNS hostnames (e.g., `homeserver.local`)
- **Multicast Transport**: All mDNS queries are sent to multicast address (IPv4: 224.0.0.251, IPv6: ff02::fb)
- **Port**: All mDNS communication uses UDP port 5353
- **Zero Configuration**: No DNS server setup required; devices announce themselves automatically
- **TTL Handling**: Resource records include Time-To-Live (TTL) values for cache management

---

**Last Updated**: February 2026  
**RFC Reference**: [RFC 6762 - Multicast DNS](https://tools.ietf.org/html/rfc6762)  
**Companion RFC 6763**: [DNS Service Discovery](https://tools.ietf.org/html/rfc6763)
