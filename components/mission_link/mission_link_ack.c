#include "mission_link_ack.h"
#include <string.h>

bool ml_ack_make_for_data(const ml_packet_t *received_data,
                          uint32_t local_node_id, ml_packet_t *out_ack)
{
    if (received_data == NULL || out_ack == NULL ||
        received_data->type != ML_PACKET_DATA ||
        received_data->message_id == 0u ||
        received_data->payload_length > ML_MAX_PAYLOAD ||
        received_data->destination != local_node_id ||
        received_data->source == ML_BROADCAST_ADDRESS ||
        received_data->source == local_node_id) {
        return false;
    }
    memset(out_ack, 0, sizeof(*out_ack));
    out_ack->type = ML_PACKET_ACK;
    out_ack->source = local_node_id;
    out_ack->destination = received_data->source;
    out_ack->message_id = received_data->message_id;
    return true;
}

bool ml_ack_accept(const ml_packet_t *ack, uint32_t local_node_id,
                   ml_outbox_t *outbox)
{
    size_t i;
    if (ack == NULL || outbox == NULL ||
        outbox->count > ML_OUTBOX_CAPACITY ||
        ack->type != ML_PACKET_ACK || ack->message_id == 0u ||
        ack->payload_length != 0u || ack->destination != local_node_id) {
        return false;
    }
    for (i = 0; i < outbox->count; ++i) {
        const ml_outbox_entry_t *entry = &outbox->entries[i];
        if (entry->packet.message_id == ack->message_id) {
            if (entry->state != ML_DELIVERY_SENT ||
                entry->packet.destination != ack->source ||
                entry->packet.destination == ML_BROADCAST_ADDRESS) {
                return false;
            }
            return ml_outbox_mark_delivered(outbox, ack->message_id);
        }
    }
    return false;
}
