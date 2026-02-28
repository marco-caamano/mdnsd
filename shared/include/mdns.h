#ifndef MDNS_H
#define MDNS_H

#include <stddef.h>
#include <stdint.h>

#include "hostdb.h"

#define MDNS_PORT 5353
#define MDNS_MAX_PACKET 1500

#define DNS_TYPE_A 1
#define DNS_TYPE_PTR 12
#define DNS_TYPE_TXT 16
#define DNS_TYPE_AAAA 28
#define DNS_TYPE_SRV 33
#define DNS_CLASS_IN 1

typedef struct {
    char name[256];
    uint16_t qtype;
    uint16_t qclass;
} dns_question_t;

int mdns_parse_query(const uint8_t *packet, size_t packet_len, dns_question_t *question);
int mdns_build_response(uint8_t *out, size_t out_len, const dns_question_t *question, const host_record_t *record);

// Build service response (SRV + TXT records)
int mdns_build_service_response(uint8_t *out, size_t out_len, const dns_question_t *question,
                                 mdns_service_t **services, size_t service_count);

#endif
