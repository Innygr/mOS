#include "mission_link_inbox.h"

#include <string.h>

static bool find_message(const ml_inbox_t *inbox,
                         uint32_t source,
                         uint32_t message_id,
                         size_t *index_out)
{
    size_t i;

    if (inbox == NULL || message_id == 0u ||
        inbox->count > ML_INBOX_CAPACITY) {
        return false;
    }

    for (i = 0; i < inbox->count; ++i) {
        if (inbox->entries[i].packet.source == source &&
            inbox->entries[i].packet.message_id == message_id) {
            if (index_out != NULL) {
                *index_out = i;
            }
            return true;
        }
    }
    return false;
}

void ml_inbox_init(ml_inbox_t *inbox)
{
    if (inbox != NULL) {
        memset(inbox, 0, sizeof(*inbox));
    }
}

bool ml_inbox_receive(ml_inbox_t *inbox, const ml_packet_t *packet)
{
    size_t i;
    ml_inbox_entry_t entry;

    if (inbox == NULL || packet == NULL ||
        packet->type != ML_PACKET_DATA ||
        packet->message_id == 0u ||
        packet->payload_length > ML_MAX_PAYLOAD ||
        inbox->count > ML_INBOX_CAPACITY ||
        inbox->count == ML_INBOX_CAPACITY) {
        return false;
    }

    /* Message IDs are scoped to a sender, not globally unique. */
    for (i = 0; i < inbox->count; ++i) {
        if (inbox->entries[i].packet.source == packet->source &&
            inbox->entries[i].packet.message_id == packet->message_id) {
            return false;
        }
    }

    memset(&entry, 0, sizeof(entry));
    entry.packet = *packet;
    entry.unread = true;
    inbox->entries[inbox->count] = entry;
    ++inbox->count;
    return true;
}

bool ml_inbox_get(const ml_inbox_t *inbox,
                  size_t index,
                  ml_inbox_entry_t *out)
{
    if (inbox == NULL || out == NULL ||
        inbox->count > ML_INBOX_CAPACITY || index >= inbox->count) {
        return false;
    }
    *out = inbox->entries[index];
    return true;
}

bool ml_inbox_mark_read(ml_inbox_t *inbox,
                        uint32_t source,
                        uint32_t message_id)
{
    size_t index;

    if (!find_message(inbox, source, message_id, &index)) {
        return false;
    }
    inbox->entries[index].unread = false;
    return true;
}

bool ml_inbox_remove(ml_inbox_t *inbox,
                     uint32_t source,
                     uint32_t message_id)
{
    size_t index;
    size_t i;

    if (!find_message(inbox, source, message_id, &index)) {
        return false;
    }

    for (i = index; i + 1u < inbox->count; ++i) {
        inbox->entries[i] = inbox->entries[i + 1u];
    }
    --inbox->count;
    memset(&inbox->entries[inbox->count], 0,
           sizeof(inbox->entries[inbox->count]));
    return true;
}
