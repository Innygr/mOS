#include "mission_link_queue.h"

#include <string.h>

static bool find_message(const ml_outbox_t *outbox,
                         uint32_t message_id,
                         size_t *index_out)
{
    size_t i;

    if (outbox == NULL || message_id == 0u) {
        return false;
    }

    for (i = 0; i < outbox->count; ++i) {
        if (outbox->entries[i].packet.message_id == message_id) {
            if (index_out != NULL) {
                *index_out = i;
            }
            return true;
        }
    }
    return false;
}

void ml_outbox_init(ml_outbox_t *outbox)
{
    if (outbox != NULL) {
        memset(outbox, 0, sizeof(*outbox));
    }
}

bool ml_outbox_enqueue(ml_outbox_t *outbox, const ml_packet_t *packet)
{
    size_t i;
    ml_outbox_entry_t entry;

    if (outbox == NULL || packet == NULL ||
        packet->type != ML_PACKET_DATA ||
        packet->message_id == 0u ||
        packet->payload_length > ML_MAX_PAYLOAD ||
        outbox->count > ML_OUTBOX_CAPACITY ||
        outbox->count == ML_OUTBOX_CAPACITY) {
        return false;
    }

    for (i = 0; i < outbox->count; ++i) {
        if (outbox->entries[i].packet.source == packet->source &&
            outbox->entries[i].packet.message_id == packet->message_id) {
            return false;
        }
    }

    memset(&entry, 0, sizeof(entry));
    entry.packet = *packet;
    entry.state = ML_DELIVERY_QUEUED;
    outbox->entries[outbox->count] = entry;
    ++outbox->count;
    return true;
}

bool ml_outbox_get(const ml_outbox_t *outbox,
                   size_t index,
                   ml_outbox_entry_t *out)
{
    if (outbox == NULL || out == NULL ||
        outbox->count > ML_OUTBOX_CAPACITY || index >= outbox->count) {
        return false;
    }
    *out = outbox->entries[index];
    return true;
}

bool ml_outbox_mark_sent(ml_outbox_t *outbox, uint32_t message_id)
{
    size_t index;
    if (!find_message(outbox, message_id, &index) ||
        outbox->entries[index].state != ML_DELIVERY_QUEUED) {
        return false;
    }
    outbox->entries[index].state = ML_DELIVERY_SENT;
    return true;
}

bool ml_outbox_mark_delivered(ml_outbox_t *outbox, uint32_t message_id)
{
    size_t index;
    if (!find_message(outbox, message_id, &index) ||
        outbox->entries[index].state != ML_DELIVERY_SENT) {
        return false;
    }
    outbox->entries[index].state = ML_DELIVERY_DELIVERED;
    return true;
}

bool ml_outbox_mark_failed(ml_outbox_t *outbox, uint32_t message_id)
{
    size_t index;
    if (!find_message(outbox, message_id, &index)) {
        return false;
    }
    if (outbox->entries[index].state != ML_DELIVERY_QUEUED &&
        outbox->entries[index].state != ML_DELIVERY_SENT) {
        return false;
    }
    outbox->entries[index].state = ML_DELIVERY_FAILED;
    return true;
}

bool ml_outbox_requeue(ml_outbox_t *outbox, uint32_t message_id)
{
    size_t index;
    if (!find_message(outbox, message_id, &index) ||
        outbox->entries[index].state != ML_DELIVERY_SENT) {
        return false;
    }
    outbox->entries[index].state = ML_DELIVERY_QUEUED;
    return true;
}

bool ml_outbox_remove_finished(ml_outbox_t *outbox, uint32_t message_id)
{
    size_t index;
    size_t i;
    ml_delivery_state_t state;

    if (!find_message(outbox, message_id, &index)) {
        return false;
    }
    state = outbox->entries[index].state;
    if (state != ML_DELIVERY_DELIVERED && state != ML_DELIVERY_FAILED) {
        return false;
    }

    for (i = index; i + 1u < outbox->count; ++i) {
        outbox->entries[i] = outbox->entries[i + 1u];
    }
    --outbox->count;
    memset(&outbox->entries[outbox->count], 0,
           sizeof(outbox->entries[outbox->count]));
    return true;
}
