#ifndef MISSION_LINK_RETRY_H
#define MISSION_LINK_RETRY_H

#include "mission_link_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ML_RETRY_CAPACITY ML_OUTBOX_CAPACITY

typedef struct {
    uint32_t message_id;
    uint32_t attempts;
    uint64_t last_tx_ms;
    bool active;
} ml_retry_entry_t;

typedef struct {
    ml_retry_entry_t entries[ML_RETRY_CAPACITY];
} ml_retry_state_t;

/* Volatile retry metadata; the caller supplies a monotonic millisecond clock. */
void ml_retry_init(ml_retry_state_t *retry);

/* Record a successful TX for a message already marked SENT in the outbox. */
bool ml_retry_note_tx(ml_retry_state_t *retry,
                      const ml_outbox_t *outbox,
                      uint32_t message_id,
                      uint64_t now_ms);

/*
 * Record a FAILED transmit attempt (radio busy, TX error, ...) for a message
 * that is still QUEUED. Failed attempts count toward the same attempt budget as
 * sent ones, so a message cannot be re-offered to a broken radio forever: once
 * attempts reaches max_attempts the message is marked FAILED immediately.
 * Returns false if the message is unknown or not QUEUED, or no slot is free.
 */
bool ml_retry_note_tx_failed(ml_retry_state_t *retry,
                             ml_outbox_t *outbox,
                             uint32_t message_id,
                             uint32_t max_attempts);

/*
 * Requeue SENT messages after an exponentially increasing ACK timeout.
 * Once attempts reaches max_attempts, the next timeout marks the message FAILED.
 * A SENT message that was never registered with ml_retry_note_tx() is adopted
 * on the first poll that sees it (counted as one attempt, timer starting at
 * now_ms), so a forgotten bookkeeping call cannot leave it SENT forever.
 * Returns the number of messages transitioned to QUEUED or FAILED.
 */
size_t ml_retry_poll(ml_retry_state_t *retry,
                     ml_outbox_t *outbox,
                     uint64_t now_ms,
                     uint32_t base_timeout_ms,
                     uint32_t max_attempts);

#ifdef __cplusplus
}
#endif

#endif
