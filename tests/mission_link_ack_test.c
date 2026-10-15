#include "mission_link_ack.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    ml_packet_t data, ack, decoded;
    ml_outbox_t outbox;
    uint8_t frame[ML_MAX_FRAME];
    size_t n;
    memset(&data, 0, sizeof(data));
    data.type = ML_PACKET_DATA; data.source = 10u; data.destination = 20u;
    data.message_id = 77u; data.payload_length = 1u; data.payload[0] = 'x';

    assert(ml_ack_make_for_data(&data, 20u, &ack));
    assert(ack.type == ML_PACKET_ACK && ack.source == 20u);
    assert(ack.destination == 10u && ack.message_id == 77u);
    assert(ack.payload_length == 0u);

    ml_outbox_init(&outbox);
    data.source = 20u; data.destination = 30u;
    assert(ml_outbox_enqueue(&outbox, &data));
    assert(ml_outbox_mark_sent(&outbox, 77u));

    /* Spoofed/wrong peer cannot mark the message delivered. */
    ack.source = 31u; ack.destination = 20u;
    assert(!ml_ack_accept(&ack, 20u, &outbox));
    assert(outbox.entries[0].state == ML_DELIVERY_SENT);

    ack.source = 30u;
    assert(ml_ack_accept(&ack, 20u, &outbox));
    assert(outbox.entries[0].state == ML_DELIVERY_DELIVERED);
    assert(!ml_ack_accept(&ack, 20u, &outbox));

    /* The ACK wire representation is an empty payload with matching message ID. */
    n = ml_packet_encode(&ack, frame, sizeof(frame));
    assert(n > 0u);
    assert(ml_packet_decode(frame, n, &decoded));
    assert(decoded.type == ML_PACKET_ACK && decoded.payload_length == 0u);
    assert(decoded.message_id == 77u);

    data.destination = ML_BROADCAST_ADDRESS;
    assert(!ml_ack_make_for_data(&data, 20u, &ack));
    puts("mission_link_ack_test: ok");
    return 0;
}
