#include "mission_link_receive.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t make_frame(uint8_t *frame, uint32_t source, uint32_t destination,
                         uint32_t message_id, uint8_t type)
{
    ml_packet_t p;
    memset(&p, 0, sizeof(p));
    p.type = type; p.ttl = 3u; p.source = source;
    p.destination = destination; p.message_id = message_id;
    p.payload_length = 2u; p.payload[0] = 'h'; p.payload[1] = 'i';
    return ml_packet_encode(&p, frame, ML_MAX_FRAME);
}

int main(void)
{
    uint8_t frame[ML_MAX_FRAME];
    ml_inbox_t inbox;
    ml_receive_report_t r;
    ml_inbox_entry_t entry;
    size_t n;
    ml_inbox_init(&inbox);

    n = make_frame(frame, 10u, 20u, 1u, ML_PACKET_DATA);
    r = ml_receive_frame(frame, n, 20u, &inbox);
    assert(r.result == ML_RECEIVE_ACCEPTED && r.ack_recommended);
    assert(inbox.count == 1u && r.packet.source == 10u);
    assert(ml_inbox_get(&inbox, 0u, &entry) && entry.unread);
    assert(entry.packet.payload_length == 2u);

    r = ml_receive_frame(frame, n, 20u, &inbox);
    assert(r.result == ML_RECEIVE_DUPLICATE && r.ack_recommended);
    assert(inbox.count == 1u);

    n = make_frame(frame, 11u, 21u, 2u, ML_PACKET_DATA);
    r = ml_receive_frame(frame, n, 20u, &inbox);
    assert(r.result == ML_RECEIVE_NOT_FOR_US && !r.ack_recommended);
    assert(inbox.count == 1u);

    n = make_frame(frame, 12u, ML_BROADCAST_ADDRESS, 3u, ML_PACKET_DATA);
    r = ml_receive_frame(frame, n, 20u, &inbox);
    assert(r.result == ML_RECEIVE_ACCEPTED && !r.ack_recommended);
    assert(inbox.count == 2u);

    n = make_frame(frame, 13u, 20u, 4u, ML_PACKET_HELLO);
    r = ml_receive_frame(frame, n, 20u, &inbox);
    assert(r.result == ML_RECEIVE_UNSUPPORTED_TYPE && inbox.count == 2u);

    n = make_frame(frame, 14u, 20u, 5u, ML_PACKET_DATA);
    frame[n - 1u] ^= 1u;
    r = ml_receive_frame(frame, n, 20u, &inbox);
    assert(r.result == ML_RECEIVE_INVALID_FRAME && !r.ack_recommended);
    assert(inbox.count == 2u);

    while (inbox.count < ML_INBOX_CAPACITY) {
        uint32_t id = (uint32_t)inbox.count + 10u;
        n = make_frame(frame, 30u, 20u, id, ML_PACKET_DATA);
        r = ml_receive_frame(frame, n, 20u, &inbox);
        assert(r.result == ML_RECEIVE_ACCEPTED);
    }
    n = make_frame(frame, 99u, 20u, 99u, ML_PACKET_DATA);
    r = ml_receive_frame(frame, n, 20u, &inbox);
    assert(r.result == ML_RECEIVE_INBOX_FULL && !r.ack_recommended);
    assert(inbox.count == ML_INBOX_CAPACITY);

    r = ml_receive_frame(NULL, 0u, 20u, &inbox);
    assert(r.result == ML_RECEIVE_INVALID_ARGUMENT);
    puts("mission_link_receive_test: ok");
    return 0;
}
