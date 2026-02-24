#include "mdns.h"

#include <arpa/inet.h>
#include <string.h>

#define DNS_FLAG_QR_RESPONSE 0x8000
#define DNS_FLAG_AA 0x0400

static uint16_t read_u16(const uint8_t *ptr) {
    return (uint16_t)((ptr[0] << 8) | ptr[1]);
}

static void write_u16(uint8_t *ptr, uint16_t value) {
    ptr[0] = (uint8_t)((value >> 8) & 0xFF);
    ptr[1] = (uint8_t)(value & 0xFF);
}

static void write_u32(uint8_t *ptr, uint32_t value) {
    ptr[0] = (uint8_t)((value >> 24) & 0xFF);
    ptr[1] = (uint8_t)((value >> 16) & 0xFF);
    ptr[2] = (uint8_t)((value >> 8) & 0xFF);
    ptr[3] = (uint8_t)(value & 0xFF);
}

static int parse_qname(const uint8_t *packet, size_t packet_len, size_t *offset, char *out, size_t out_len) {
    size_t pos = *offset;
    size_t written = 0;

    if (pos >= packet_len) {
        return -1;
    }

    while (pos < packet_len) {
        uint8_t label_len = packet[pos++];

        if (label_len == 0) {
            if (written == 0) {
                return -1;
            }
            out[written] = '\0';
            *offset = pos;
            return 0;
        }

        if ((label_len & 0xC0) != 0) {
            return -1;
        }

        if (pos + label_len > packet_len) {
            return -1;
        }

        if (written + label_len + 1 >= out_len) {
            return -1;
        }

        memcpy(&out[written], &packet[pos], label_len);
        written += label_len;
        pos += label_len;

        out[written++] = '.';
    }

    return -1;
}

static int encode_qname(const char *name, uint8_t *out, size_t out_len, size_t *written_out) {
    const char *cursor = name;
    size_t written = 0;

    while (*cursor != '\0') {
        const char *dot = strchr(cursor, '.');
        size_t label_len = dot ? (size_t)(dot - cursor) : strlen(cursor);

        if (label_len == 0 || label_len > 63) {
            return -1;
        }
        if (written + 1 + label_len >= out_len) {
            return -1;
        }

        out[written++] = (uint8_t)label_len;
        memcpy(&out[written], cursor, label_len);
        written += label_len;

        if (dot == NULL) {
            break;
        }
        cursor = dot + 1;
    }

    if (written >= out_len) {
        return -1;
    }
    out[written++] = 0;

    *written_out = written;
    return 0;
}

int mdns_parse_query(const uint8_t *packet, size_t packet_len, dns_question_t *question) {
    uint16_t flags;
    uint16_t qdcount;
    size_t offset;

    if (packet == NULL || question == NULL || packet_len < 12) {
        return -1;
    }

    flags = read_u16(&packet[2]);
    if (flags & 0x8000) {
        return 0;
    }

    qdcount = read_u16(&packet[4]);
    if (qdcount == 0) {
        return 0;
    }

    offset = 12;
    if (parse_qname(packet, packet_len, &offset, question->name, sizeof(question->name)) != 0) {
        return -1;
    }

    if (offset + 4 > packet_len) {
        return -1;
    }

    question->qtype = read_u16(&packet[offset]);
    question->qclass = read_u16(&packet[offset + 2]);

    return 1;
}

int mdns_build_response(uint8_t *out, size_t out_len, const dns_question_t *question, const host_record_t *record) {
    size_t qname_len;
    size_t offset;
    uint16_t answers = 0;

    if (out == NULL || question == NULL || record == NULL || out_len < 12) {
        return -1;
    }

    memset(out, 0, out_len);
    write_u16(&out[0], 0);
    write_u16(&out[2], DNS_FLAG_QR_RESPONSE | DNS_FLAG_AA);
    write_u16(&out[4], 1);

    offset = 12;
    if (encode_qname(question->name, &out[offset], out_len - offset, &qname_len) != 0) {
        return -1;
    }
    offset += qname_len;

    if (offset + 4 > out_len) {
        return -1;
    }
    write_u16(&out[offset], question->qtype);
    write_u16(&out[offset + 2], DNS_CLASS_IN);
    offset += 4;

    if (question->qtype == DNS_TYPE_A && record->has_ipv4) {
        if (offset + 2 + 2 + 2 + 4 + 2 + 4 > out_len) {
            return -1;
        }
        write_u16(&out[offset], 0xC00C);
        write_u16(&out[offset + 2], DNS_TYPE_A);
        write_u16(&out[offset + 4], DNS_CLASS_IN_FLUSH);
        write_u32(&out[offset + 6], MDNS_DEFAULT_TTL);
        write_u16(&out[offset + 10], 4);
        memcpy(&out[offset + 12], &record->ipv4, 4);
        offset += 16;
        answers = 1;
    } else if (question->qtype == DNS_TYPE_AAAA && record->has_ipv6) {
        if (offset + 2 + 2 + 2 + 4 + 2 + 16 > out_len) {
            return -1;
        }
        write_u16(&out[offset], 0xC00C);
        write_u16(&out[offset + 2], DNS_TYPE_AAAA);
        write_u16(&out[offset + 4], DNS_CLASS_IN_FLUSH);
        write_u32(&out[offset + 6], MDNS_DEFAULT_TTL);
        write_u16(&out[offset + 10], 16);
        memcpy(&out[offset + 12], &record->ipv6, 16);
        offset += 28;
        answers = 1;
    } else {
        return 0;
    }

    write_u16(&out[6], answers);

    return (int)offset;
}
