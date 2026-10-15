#include "mission_link_retry.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    ml_outbox_t outbox;
    ml_retry_state_t retry;
    ml_packet_t packet;

    ml_outbox_init(&outbox);
    ml_retry_init(&retry);
    memset(&packet, 0, sizeof(packet));
    packet.type = ML_PACKET_DATA;
    packet.source = 1u;
    packet.destination = 2u;
    packet.message_id = 77u;
    packet.payload_length = 1u;
    packet.payload[0] = 'R';
    assert(ml_outbox_enqueue(&outbox, &packet));

    /* A failed TX is not an ACK timeout and must not start retry timing. */
    assert(!ml_retry_note_tx(&retry, &outbox, 77u, 100u));
    assert(ml_outbox_mark_sent(&outbox, 77u));
    assert(ml_retry_note_tx(&retry, &outbox, 77u, 100u));
    assert(ml_retry_poll(&retry, &outbox, 1099u, 1000u, 3u) == 0u);
    assert(outbox.entries[0].state == ML_DELIVERY_SENT);

    /* First timeout requeues; next successful TX increments attempt count. */
    assert(ml_retry_poll(&retry, &outbox, 1100u, 1000u, 3u) == 1u);
    assert(outbox.entries[0].state == ML_DELIVERY_QUEUED);
    assert(ml_retry_poll(&retry, &outbox, 9000u, 1000u, 3u) == 0u);
    assert(ml_outbox_mark_sent(&outbox, 77u));
    assert(ml_retry_note_tx(&retry, &outbox, 77u, 9000u));
    assert(ml_retry_poll(&retry, &outbox, 10999u, 1000u, 3u) == 0u);
    assert(ml_retry_poll(&retry, &outbox, 11000u, 1000u, 3u) == 1u);
    assert(outbox.entries[0].state == ML_DELIVERY_QUEUED);

    /* Third attempt uses a 4x timeout; exhaustion fails instead of looping. */
    assert(ml_outbox_mark_sent(&outbox, 77u));
    assert(ml_retry_note_tx(&retry, &outbox, 77u, 12000u));
    assert(ml_retry_poll(&retry, &outbox, 15999u, 1000u, 3u) == 0u);
    assert(ml_retry_poll(&retry, &outbox, 16000u, 1000u, 3u) == 1u);
    assert(outbox.entries[0].state == ML_DELIVERY_FAILED);
    assert(ml_retry_poll(&retry, &outbox, 99999u, 1000u, 3u) == 0u);

    puts("mission_link_retry_test: ok");
    return 0;
}
