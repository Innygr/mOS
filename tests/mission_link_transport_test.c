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

    /* A too-small TX buffer must not change outbox state or publish an ID. */
    {
        uint32_t untouched_id = 1234u;
        assert(ml_transport_prepare_next(&outbox, frame, 1u, &untouched_id) == 0u);
        assert(untouched_id == 1234u);
    }

    /* A corrupted frame is rejected and cannot enqueue or generate an ACK. */
    n = encode_data(frame, 13u, 20u, 89u, ML_PACKET_DATA);
    assert(n > 0u);
    frame[n - 1u] ^= 0x01u;
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                              ack_frame, sizeof(ack_frame));
    assert(rx.kind == ML_TRANSPORT_RX_INVALID);
    assert(inbox.count == 2u);
    assert(rx.ack_frame_length == 0u);

    /* Unsupported packet types are surfaced without inbox insertion or ACK. */
    n = encode_data(frame, 14u, 20u, 90u, ML_PACKET_ROUTE);
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                              ack_frame, sizeof(ack_frame));
    assert(rx.kind == ML_TRANSPORT_RX_DATA);
    assert(rx.data.result == ML_RECEIVE_UNSUPPORTED_TYPE);
    assert(rx.ack_frame_length == 0u);
    assert(inbox.count == 2u);

    /* If the ACK output buffer is too small, the message is still stored;
       retransmitting it later can generate an ACK without duplicating inbox data. */
    n = encode_data(frame, 15u, 20u, 91u, ML_PACKET_DATA);
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox, ack_frame, 1u);
    assert(rx.data.result == ML_RECEIVE_ACCEPTED);
    assert(rx.ack_frame_length == 0u);
    assert(inbox.count == 3u);
    rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                              ack_frame, sizeof(ack_frame));
    assert(rx.data.result == ML_RECEIVE_DUPLICATE);
    assert(rx.ack_frame_length > 0u);
    assert(inbox.count == 3u);

    /* Fill the inbox; a new message is rejected and must not be ACKed. */
    {
        uint32_t message_id;
        for (message_id = 100u; message_id < 105u; ++message_id) {
            n = encode_data(frame, 16u, 20u, message_id, ML_PACKET_DATA);
            rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                                      ack_frame, sizeof(ack_frame));
            assert(rx.data.result == ML_RECEIVE_ACCEPTED);
        }
        assert(inbox.count == ML_INBOX_CAPACITY);
        n = encode_data(frame, 16u, 20u, 105u, ML_PACKET_DATA);
        rx = ml_transport_receive(frame, n, 20u, &inbox, &outbox,
                                  ack_frame, sizeof(ack_frame));
        assert(rx.data.result == ML_RECEIVE_INBOX_FULL);
        assert(rx.ack_frame_length == 0u);
        assert(inbox.count == ML_INBOX_CAPACITY);
    }

    puts("mission_link_transport_test: ok");
    return 0;
}
