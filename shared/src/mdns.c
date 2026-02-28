#include "mdns.h"

#include <arpa/inet.h>
#include <stdio.h>
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
    uint16_t qdcount;
    size_t offset;

    if (packet == NULL || question == NULL || packet_len < 12) {
        return -1;
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
        write_u16(&out[offset + 4], DNS_CLASS_IN);
        write_u32(&out[offset + 6], 120);
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
        write_u16(&out[offset + 4], DNS_CLASS_IN);
        write_u32(&out[offset + 6], 120);
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

// Helper: Write answer header (name, type, class, ttl, rdlength placeholder)
// Returns offset to write RDATA, or -1 on error
static int write_answer_header(uint8_t *out, size_t out_len, size_t *offset,
                                const char *name, uint16_t type, uint32_t ttl) {
    size_t qname_len;
    
    // Write name
    if (encode_qname(name, &out[*offset], out_len - *offset, &qname_len) != 0) {
        return -1;
    }
    *offset += qname_len;
    
    // Write TYPE, CLASS, TTL, RDLENGTH placeholder
    if (*offset + 10 > out_len) {
        return -1;
    }
    
    write_u16(&out[*offset], type);
    write_u16(&out[*offset + 2], DNS_CLASS_IN);
    write_u32(&out[*offset + 4], ttl);
    *offset += 8;
    
    // Return position for RDLENGTH
    return (int)*offset;
}

// Helper: Encode SRV record RDATA
static int encode_srv_rdata(uint8_t *out, size_t out_len, size_t *offset,
                             const mdns_service_t *svc) {
    size_t target_len;
    
    // Check space for priority, weight, port
    if (*offset + 6 > out_len) {
        return -1;
    }
    
    write_u16(&out[*offset], svc->priority);
    write_u16(&out[*offset + 2], svc->weight);
    write_u16(&out[*offset + 4], svc->port);
    *offset += 6;
    
    // Encode target hostname
    if (encode_qname(svc->target_host, &out[*offset], out_len - *offset, &target_len) != 0) {
        return -1;
    }
    *offset += target_len;
    
    return 0;
}

// Helper: Encode TXT record RDATA
static int encode_txt_rdata(uint8_t *out, size_t out_len, size_t *offset,
                             const mdns_service_t *svc) {
    // If no TXT records, write a single empty string (length 0)
    if (svc->txt_kv_count == 0 || svc->txt_kv == NULL) {
        if (*offset + 1 > out_len) {
            return -1;
        }
        out[(*offset)++] = 0;
        return 0;
    }
    
    // Write each TXT record as length-prefixed string
    for (size_t i = 0; i < svc->txt_kv_count; i++) {
        size_t txt_len = strlen(svc->txt_kv[i]);
        if (txt_len > 255) {
            txt_len = 255;  // Truncate if too long
        }
        
        if (*offset + 1 + txt_len > out_len) {
            return -1;
        }
        
        out[(*offset)++] = (uint8_t)txt_len;
        memcpy(&out[*offset], svc->txt_kv[i], txt_len);
        *offset += txt_len;
    }
    
    return 0;
}

// Build service response with SRV + TXT records for each service
int mdns_build_service_response(uint8_t *out, size_t out_len, const dns_question_t *question,
                                 mdns_service_t **services, size_t service_count) {
    size_t qname_len;
    size_t offset;
    uint16_t answer_count = 0;
    
    if (out == NULL || question == NULL || out_len < 12) {
        return -1;
    }
    
    if (service_count == 0 || services == NULL) {
        return 0;  // No services to return
    }
    
    // DNS header
    memset(out, 0, out_len);
    write_u16(&out[0], 0);  // ID
    write_u16(&out[2], DNS_FLAG_QR_RESPONSE | DNS_FLAG_AA);
    write_u16(&out[4], 1);  // QDCOUNT
    
    // Question section
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
    
    // Answer section: SRV + TXT for each service
    for (size_t i = 0; i < service_count; i++) {
        mdns_service_t *svc = services[i];
        char service_fqdn[512];
        int rdlength_pos;
        size_t rdata_start;
        size_t rdata_len;
        
        // Construct service FQDN
        int written = snprintf(service_fqdn, sizeof(service_fqdn), "%s.%s.%s",
                              svc->instance, svc->service_type, svc->domain);
        if (written < 0 || (size_t)written >= sizeof(service_fqdn)) {
            continue;  // Skip this service
        }
        
        // Write SRV record
        rdlength_pos = write_answer_header(out, out_len, &offset, service_fqdn,
                                           DNS_TYPE_SRV, svc->ttl);
        if (rdlength_pos < 0) {
            break;  // Out of space
        }
        
        rdata_start = offset;
        if (encode_srv_rdata(out, out_len, &offset, svc) != 0) {
            break;  // Out of space
        }
        
        // Fill in RDLENGTH
        rdata_len = offset - rdata_start;
        write_u16(&out[rdlength_pos], (uint16_t)rdata_len);
        answer_count++;
        
        // Write TXT record
        rdlength_pos = write_answer_header(out, out_len, &offset, service_fqdn,
                                           DNS_TYPE_TXT, svc->ttl);
        if (rdlength_pos < 0) {
            break;  // Out of space
        }
        
        rdata_start = offset;
        if (encode_txt_rdata(out, out_len, &offset, svc) != 0) {
            break;  // Out of space
        }
        
        // Fill in RDLENGTH
        rdata_len = offset - rdata_start;
        write_u16(&out[rdlength_pos], (uint16_t)rdata_len);
        answer_count++;
    }
    
    // Update ANCOUNT
    write_u16(&out[6], answer_count);
    
    return answer_count > 0 ? (int)offset : 0;
}
