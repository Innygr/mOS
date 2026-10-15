#include "mission_link_transport.h"
#include <string.h>

size_t ml_transport_prepare_next(const ml_outbox_t *outbox, uint8_t *frame,
                                 size_t frame_capacity, uint32_t *message_id_out)
{
    size_t i;
    if (outbox == NULL || frame == NULL || message_id_out == NULL ||
        outbox->count > ML_OUTBOX_CAPACITY) return 0u;
    for (i = 0; i < outbox->count; ++i) {
        const ml_outbox_entry_t *entry = &outbox->entries[i];
        size_t length;
        if (entry->state != ML_DELIVERY_QUEUED) continue;
        length = ml_packet_encode(&entry->packet, frame, frame_capacity);
        if (length == 0u) return 0u;
        *message_id_out = entry->packet.message_id;
        return length;
    }
    return 0u;
}

bool ml_transport_report_tx(ml_outbox_t *outbox, uint32_t message_id,
                             bool tx_succeeded)
{
    size_t i;
    if (outbox == NULL || outbox->count > ML_OUTBOX_CAPACITY ||
        message_id == 0u) return false;
    for (i = 0; i < outbox->count; ++i) {
        if (outbox->entries[i].packet.message_id == message_id) {
            if (outbox->entries[i].state != ML_DELIVERY_QUEUED) return false;
            if (!tx_succeeded) return true;
            return ml_outbox_mark_sent(outbox, message_id);
        }
    }
    return false;
}

ml_transport_rx_report_t ml_transport_receive(const uint8_t *frame,
                                              size_t frame_length,
                                              uint32_t local_node_id,
                                              ml_inbox_t *inbox,
                                              ml_outbox_t *outbox,
                                              uint8_t *ack_frame,
                                              size_t ack_capacity)
{
    ml_transport_rx_report_t report;
    ml_packet_t packet;
    memset(&report, 0, sizeof(report));
    report.kind = ML_TRANSPORT_RX_INVALID;

    if (frame == NULL || inbox == NULL || outbox == NULL ||
        !ml_packet_decode(frame, frame_length, &packet)) return report;

    if (packet.type == ML_PACKET_ACK) {
        report.kind = ml_ack_accept(&packet, local_node_id, outbox)
                          ? ML_TRANSPORT_RX_ACK_ACCEPTED
                          : ML_TRANSPORT_RX_ACK_REJECTED;
        return report;
    }

    report.data = ml_receive_frame(frame, frame_length, local_node_id, inbox);
    report.kind = ML_TRANSPORT_RX_DATA;
    if (report.data.ack_recommended && ack_frame != NULL) {
        ml_packet_t ack;
        if (ml_ack_make_for_data(&report.data.packet, local_node_id, &ack)) {
            report.ack_frame_length =
                ml_packet_encode(&ack, ack_frame, ack_capacity);
        }
    }
    return report;
}
