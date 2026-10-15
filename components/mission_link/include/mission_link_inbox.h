#ifndef MISSION_LINK_INBOX_H
#define MISSION_LINK_INBOX_H

#include "mission_link.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ML_INBOX_CAPACITY 8u

typedef struct {
    ml_packet_t packet;
    bool unread;
} ml_inbox_entry_t;

typedef struct {
    size_t count;
    ml_inbox_entry_t entries[ML_INBOX_CAPACITY];
} ml_inbox_t;

/* Volatile, fixed-capacity inbox. Not thread-safe or persistent. */
void ml_inbox_init(ml_inbox_t *inbox);

/*
 * Accept a validated DATA packet. Duplicate (source, message_id) pairs and
 * invalid packets are rejected. The packet codec should validate the frame
 * before this API is called.
 */
bool ml_inbox_receive(ml_inbox_t *inbox, const ml_packet_t *packet);

/* Retrieve by zero-based insertion order. */
bool ml_inbox_get(const ml_inbox_t *inbox,
                  size_t index,
                  ml_inbox_entry_t *out);

/* Mark an existing message as read, addressed by sender and message ID. */
bool ml_inbox_mark_read(ml_inbox_t *inbox,
                        uint32_t source,
                        uint32_t message_id);

/* Remove a message, addressed by sender and message ID. */
bool ml_inbox_remove(ml_inbox_t *inbox,
                     uint32_t source,
                     uint32_t message_id);

#ifdef __cplusplus
}
#endif

#endif
