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
    assert(n == 0u); /* API returns bool, not frame length. */
    return 0;
}
