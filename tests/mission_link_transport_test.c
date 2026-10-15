#include "mission_link_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t encode_data(uint8_t *frame, uint32_t source, uint32_t destination,
                          uint32_t id, uint8_t type)
{
    ml_packet_t p;
    memset(&p, 0, sizeof(p));
    p.type = type; p.source = source; p.destination = destination;
    p.message_id = id; p.payload_length = 1u; p.payload[0] = 'A';
    return ml_packet_encode(&p, frame, ML_MAX_FRAME);
}

int main(void)
{
    ml_outbox_t outbox;
    ml_inbox_t inbox;
    ml_packet_t p, decoded;
    ml_transport_rx_report_t rx;
    uint8_t frame[ML_MAX_FRAME], ack_frame[ML_MAX_FRAME];
    uint32_t id = 0u;
    size_t n;

    ml_outbox_init(&outbox);
    ml_inbox_init(&inbox);
    memset(&p, 0, sizeof(p));
    p.type = ML_PACKET_DATA; p.source = 20u; p.destination = 40u;
    p.message_id = 7u; p.payload_length = 1u; p.payload[0] = 'x';
    assert(ml_outbox_enqueue(&outbox, &p));

    n = ml_transport_prepare_next(&outbox, frame, sizeof(frame), &id);
    assert(n > 0u && id == 7u);
    assert(outbox.entries[0].state == ML_DELIVERY_QUEUED);
    assert(ml_packet_decode(frame, n, &decoded));
    assert(decoded.message_id == 7u);
    assert(ml_transport_report_tx(&outbox, 7u, false));
    assert(outbox.entries[0].state == ML_DELIVERY_QUEUED);
    assert(ml_transport_report_tx(&outbox, 7u, true));
    assert(outbox.entries[0].state == ML_DELIVERY_SENT);

    /* Receive unicast DATA and build a valid ACK frame. */
    n = encode_data(frame, 10u, 20u, 55u, ML_PACKET_DATA);
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                              ack_frame, sizeof(ack_frame));
    assert(rx.kind == ML_TRANSPORT_RX_DATA);
    assert(rx.data.result == ML_RECEIVE_ACCEPTED);
    assert(rx.ack_frame_length > 0u);
    assert(ml_packet_decode(ack_frame, rx.ack_frame_length, &decoded));
    assert(decoded.type == ML_PACKET_ACK && decoded.message_id == 55u);
    assert(decoded.source == 20u && decoded.destination == 10u);

    /* Repeated DATA is not inserted twice but gets another ACK. */
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                              ack_frame, sizeof(ack_frame));
    assert(rx.data.result == ML_RECEIVE_DUPLICATE);
    assert(rx.ack_frame_length > 0u && inbox.count == 1u);

    /* A valid correlated ACK updates only its matching SENT message. */
    {
        ml_packet_t ack;
        memset(&ack, 0, sizeof(ack));
        ack.type = ML_PACKET_ACK; ack.source = 40u;
        ack.destination = 20u; ack.message_id = 7u;
        n = ml_packet_encode(&ack, frame, sizeof(frame));
    }
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                              ack_frame, sizeof(ack_frame));
    assert(rx.kind == ML_TRANSPORT_RX_ACK_ACCEPTED);
    assert(outbox.entries[0].state == ML_DELIVERY_DELIVERED);

    /* A wrong-peer ACK is rejected and cannot change state. */
    p.message_id = 8u; p.destination = 40u;
    assert(ml_outbox_enqueue(&outbox, &p));
    assert(ml_outbox_mark_sent(&outbox, 8u));
    {
        ml_packet_t ack;
        memset(&ack, 0, sizeof(ack));
        ack.type = ML_PACKET_ACK; ack.source = 41u;
        ack.destination = 20u; ack.message_id = 8u;
        n = ml_packet_encode(&ack, frame, sizeof(frame));
    }
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                              ack_frame, sizeof(ack_frame));
    assert(rx.kind == ML_TRANSPORT_RX_ACK_REJECTED);
    assert(outbox.entries[1].state == ML_DELIVERY_SENT);

    /* Broadcast DATA is accepted without an ACK. */
    n = encode_data(frame, 12u, ML_BROADCAST_ADDRESS, 88u, ML_PACKET_DATA);
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                              ack_frame, sizeof(ack_frame));
    assert(rx.data.result == ML_RECEIVE_ACCEPTED);
    assert(rx.ack_frame_length == 0u);

    puts("mission_link_transport_test: ok");
    return 0;
}
