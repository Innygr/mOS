#include "mission_link_inbox.h"

#include <assert.h>
#include <string.h>

static ml_packet_t make_packet(uint32_t source, uint32_t message_id,
                               const char *text)
{
    ml_packet_t packet;
    size_t length = strlen(text);

    memset(&packet, 0, sizeof(packet));
    packet.type = ML_PACKET_DATA;
    packet.source = source;
    packet.destination = 42u;
    packet.message_id = message_id;
    packet.ttl = 4u;
    packet.payload_length = (uint16_t)length;
    memcpy(packet.payload, text, length);
    return packet;
}

int main(void)
{
    ml_inbox_t inbox;
    ml_inbox_entry_t entry;
    ml_packet_t packet;
    size_t i;

    ml_inbox_init(&inbox);
    assert(inbox.count == 0u);
    assert(!ml_inbox_receive(NULL, &packet));
    assert(!ml_inbox_receive(&inbox, NULL));

    packet = make_packet(10u, 1u, "hello");
    assert(ml_inbox_receive(&inbox, &packet));
    assert(inbox.count == 1u);
    assert(ml_inbox_get(&inbox, 0u, &entry));
    assert(entry.unread);
    assert(entry.packet.source == 10u);
    assert(entry.packet.message_id == 1u);
    assert(entry.packet.payload_length == 5u);
    assert(memcmp(entry.packet.payload, "hello", 5u) == 0);

    /* Duplicate suppression is scoped to sender + message ID. */
    assert(!ml_inbox_receive(&inbox, &packet));
    packet = make_packet(11u, 1u, "other sender");
    assert(ml_inbox_receive(&inbox, &packet));
    assert(inbox.count == 2u);

    assert(ml_inbox_mark_read(&inbox, 10u, 1u));
    assert(ml_inbox_get(&inbox, 0u, &entry));
    assert(!entry.unread);
    assert(!ml_inbox_mark_read(&inbox, 99u, 1u));
    assert(!ml_inbox_mark_read(&inbox, 10u, 0u));

    packet = make_packet(12u, 0u, "zero ID");
    assert(!ml_inbox_receive(&inbox, &packet));
    packet = make_packet(12u, 2u, "not data");
    packet.type = ML_PACKET_ACK;
    assert(!ml_inbox_receive(&inbox, &packet));
    packet = make_packet(12u, 2u, "bad length");
    packet.payload_length = (uint16_t)(ML_MAX_PAYLOAD + 1u);
    assert(!ml_inbox_receive(&inbox, &packet));

    for (i = inbox.count; i < ML_INBOX_CAPACITY; ++i) {
        packet = make_packet((uint32_t)(100u + i), (uint32_t)(i + 1u), "x");
        assert(ml_inbox_receive(&inbox, &packet));
    }
    assert(inbox.count == ML_INBOX_CAPACITY);
    packet = make_packet(999u, 999u, "full");
    assert(!ml_inbox_receive(&inbox, &packet));
    assert(!ml_inbox_get(&inbox, ML_INBOX_CAPACITY, &entry));

    assert(ml_inbox_remove(&inbox, 10u, 1u));
    assert(inbox.count == ML_INBOX_CAPACITY - 1u);
    assert(!ml_inbox_remove(&inbox, 10u, 1u));
    assert(ml_inbox_remove(&inbox, 11u, 1u));
    assert(inbox.count == ML_INBOX_CAPACITY - 2u);

    inbox.count = ML_INBOX_CAPACITY + 1u;
    assert(!ml_inbox_get(&inbox, 0u, &entry));
    assert(!ml_inbox_mark_read(&inbox, 10u, 1u));
    assert(!ml_inbox_receive(&inbox, &packet));

    ml_inbox_init(&inbox);
    assert(inbox.count == 0u);
    assert(!ml_inbox_get(&inbox, 0u, &entry));
    return 0;
}
