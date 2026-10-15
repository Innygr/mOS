#include "mission_link_receive.h"

static bool inbox_has_message(const ml_inbox_t *inbox, uint32_t source,
                              uint32_t message_id)
{
    size_t i;
    if (inbox == NULL || inbox->count > ML_INBOX_CAPACITY) return false;
    for (i = 0; i < inbox->count; ++i) {
        if (inbox->entries[i].packet.source == source &&
            inbox->entries[i].packet.message_id == message_id) return true;
    }
    return false;
}

static ml_receive_report_t make_report(ml_receive_result_t result, bool ack,
                                       const ml_packet_t *packet)
{
    ml_receive_report_t out = {0};
    out.result = result;
    out.ack_recommended = ack;
    if (packet != NULL) out.packet = *packet;
    return out;
}

ml_receive_report_t ml_receive_frame(const uint8_t *frame, size_t frame_length,
                                    uint32_t local_node_id, ml_inbox_t *inbox)
{
    ml_packet_t packet;
    bool unicast;

    if (frame == NULL || inbox == NULL || inbox->count > ML_INBOX_CAPACITY)
        return make_report(ML_RECEIVE_INVALID_ARGUMENT, false, NULL);
    if (!ml_packet_decode(frame, frame_length, &packet))
        return make_report(ML_RECEIVE_INVALID_FRAME, false, NULL);
    if (packet.type != ML_PACKET_DATA)
        return make_report(ML_RECEIVE_UNSUPPORTED_TYPE, false, &packet);
    if (packet.destination != local_node_id &&
        packet.destination != ML_BROADCAST_ADDRESS)
        return make_report(ML_RECEIVE_NOT_FOR_US, false, &packet);
    if (packet.message_id == 0u || packet.payload_length > ML_MAX_PAYLOAD)
        return make_report(ML_RECEIVE_INVALID_FRAME, false, &packet);

    unicast = packet.destination != ML_BROADCAST_ADDRESS;
    if (inbox_has_message(inbox, packet.source, packet.message_id))
        return make_report(ML_RECEIVE_DUPLICATE, unicast, &packet);
    if (!ml_inbox_receive(inbox, &packet))
        return make_report(ML_RECEIVE_INBOX_FULL, false, &packet);
    return make_report(ML_RECEIVE_ACCEPTED, unicast, &packet);
}
