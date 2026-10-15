#ifndef MISSION_LINK_QUEUE_H
#define MISSION_LINK_QUEUE_H

#include "mission_link.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ML_OUTBOX_CAPACITY 8u

typedef enum {
    ML_DELIVERY_QUEUED = 0,
    ML_DELIVERY_SENT = 1,
    ML_DELIVERY_DELIVERED = 2,
    ML_DELIVERY_FAILED = 3
} ml_delivery_state_t;

typedef struct {
    ml_packet_t packet;
    ml_delivery_state_t state;
} ml_outbox_entry_t;

typedef struct {
    size_t count;
    ml_outbox_entry_t entries[ML_OUTBOX_CAPACITY];
} ml_outbox_t;

/* Volatile, fixed-capacity outbox. It is not thread-safe or persistent. */
void ml_outbox_init(ml_outbox_t *outbox);

/* Enqueue a DATA packet; duplicate source/message IDs are rejected. */
bool ml_outbox_enqueue(ml_outbox_t *outbox, const ml_packet_t *packet);

/* Retrieve an entry by zero-based position, preserving insertion order. */
bool ml_outbox_get(const ml_outbox_t *outbox,
                   size_t index,
                   ml_outbox_entry_t *out);

/* State changes require explicit confirmation from the caller/transport. */
bool ml_outbox_mark_sent(ml_outbox_t *outbox, uint32_t message_id);
bool ml_outbox_mark_delivered(ml_outbox_t *outbox, uint32_t message_id);
bool ml_outbox_mark_failed(ml_outbox_t *outbox, uint32_t message_id);
bool ml_outbox_requeue(ml_outbox_t *outbox, uint32_t message_id);

/* Only DELIVERED or FAILED entries may be removed. */
bool ml_outbox_remove_finished(ml_outbox_t *outbox, uint32_t message_id);

#ifdef __cplusplus
}
#endif

#endif
