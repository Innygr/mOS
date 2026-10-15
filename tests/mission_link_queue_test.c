#include "mission_link_queue.h"

#include <assert.h>
#include <string.h>

static ml_packet_t packet(uint32_t source, uint32_t id)
{
    ml_packet_t p;
    memset(&p, 0, sizeof(p));
    p.type = ML_PACKET_DATA;
    p.source = source;
    p.destination = 42u;
    p.message_id = id;
    p.payload_length = 2u;
    p.payload[0] = 'h';
    p.payload[1] = 'i';
    return p;
}

int main(void)
{
    ml_outbox_t outbox;
    ml_outbox_entry_t entry;
    ml_packet_t p;
    size_t i;

    ml_outbox_init(&outbox);
    assert(outbox.count == 0u);
    assert(!ml_outbox_get(&outbox, 0u, &entry));
    p = packet(7u, 1u);
    assert(!ml_outbox_enqueue(NULL, &p));

    assert(ml_outbox_enqueue(&outbox, &p));
    p = packet(8u, 1u);
    assert(!ml_outbox_enqueue(&outbox, &p));
    assert(!ml_outbox_mark_delivered(&outbox, 1u));
    assert(!ml_outbox_mark_sent(&outbox, 99u));
    assert(ml_outbox_get(&outbox, 0u, &entry));
    assert(entry.state == ML_DELIVERY_QUEUED);
    assert(entry.packet.payload_length == 2u);

    assert(ml_outbox_mark_sent(&outbox, 1u));
    assert(!ml_outbox_mark_sent(&outbox, 1u));
    assert(ml_outbox_requeue(&outbox, 1u));
    assert(ml_outbox_mark_sent(&outbox, 1u));
    assert(ml_outbox_mark_delivered(&outbox, 1u));
    assert(!ml_outbox_mark_failed(&outbox, 1u));
    assert(!ml_outbox_requeue(&outbox, 1u));
    assert(ml_outbox_remove_finished(&outbox, 1u));
    assert(outbox.count == 0u);

    p = packet(7u, 2u);
    p.type = ML_PACKET_HELLO;
    assert(!ml_outbox_enqueue(&outbox, &p));
    p.type = ML_PACKET_DATA;
    p.message_id = 0u;
    assert(!ml_outbox_enqueue(&outbox, &p));
    p.message_id = 2u;
    p.payload_length = (uint16_t)(ML_MAX_PAYLOAD + 1u);
    assert(!ml_outbox_enqueue(&outbox, &p));

    p = packet(7u, 10u);
    assert(ml_outbox_enqueue(&outbox, &p));
    p = packet(7u, 11u);
    assert(ml_outbox_enqueue(&outbox, &p));
    assert(ml_outbox_mark_failed(&outbox, 10u));
    assert(ml_outbox_remove_finished(&outbox, 10u));
    assert(ml_outbox_get(&outbox, 0u, &entry));
    assert(entry.packet.message_id == 11u);

    for (i = outbox.count; i < ML_OUTBOX_CAPACITY; ++i) {
        p = packet(7u, (uint32_t)(20u + i));
        assert(ml_outbox_enqueue(&outbox, &p));
    }
    assert(outbox.count == ML_OUTBOX_CAPACITY);
    p = packet(7u, 100u);
    assert(!ml_outbox_enqueue(&outbox, &p));

    ml_outbox_init(&outbox);
    assert(outbox.count == 0u);
    assert(!ml_outbox_mark_failed(&outbox, 1u));
    assert(!ml_outbox_remove_finished(&outbox, 1u));

    outbox.count = ML_OUTBOX_CAPACITY + 1u;
    assert(!ml_outbox_get(&outbox, 0u, &entry));
    assert(!ml_outbox_mark_sent(&outbox, 1u));
    assert(!ml_outbox_enqueue(&outbox, &p));
    return 0;
}
