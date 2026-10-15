#ifndef MISSION_LINK_SERVICE_H
#define MISSION_LINK_SERVICE_H

#include "mission_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal text-message service above the frame codec.
 * This module does not transmit packets, persist state, or authenticate peers.
 */
typedef struct {
    uint32_t local_node_id;
    uint32_t next_message_id;
} ml_service_t;

/* A first_message_id of zero is normalized to 1. */
bool ml_service_init(ml_service_t *service,
                     uint32_t local_node_id,
                     uint32_t first_message_id);

/*
 * Build a DATA packet from a NUL-terminated text string.
 * Returns false for invalid arguments or text longer than ML_MAX_PAYLOAD.
 * Message IDs are nonzero and wrap from UINT32_MAX to 1.
 */
bool ml_service_make_text(ml_service_t *service,
                          uint32_t destination,
                          uint8_t ttl,
                          const char *text,
                          ml_packet_t *out);

/*
 * Copy a DATA packet payload to a NUL-terminated output string.
 * Embedded NUL bytes are rejected; payload bytes otherwise remain opaque
 * (the caller is responsible for UTF-8 validation if required).
 */
bool ml_service_read_text(const ml_packet_t *packet,
                          char *out,
                          size_t out_capacity);

#ifdef __cplusplus
}
#endif

#endif
