#include "mission_link_retry.h"
#include "mission_link_transport.h"

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

    /* Persistent TX failure must exhaust the attempt budget, not loop forever. */
    {
        ml_outbox_t ob;
        ml_retry_state_t rt;
        ml_packet_t p;
        uint8_t frame[ML_MAX_FRAME];
        uint32_t id = 0u;
        unsigned offered = 0u;

        ml_outbox_init(&ob);
        ml_retry_init(&rt);
        memset(&p, 0, sizeof(p));
        p.type = ML_PACKET_DATA;
        p.source = 1u;
        p.destination = 2u;
        p.message_id = 90u;
        assert(ml_outbox_enqueue(&ob, &p));

        /* Not QUEUED / unknown / zero budget are rejected without side effects. */
        assert(!ml_retry_note_tx_failed(&rt, &ob, 12345u, 3u));
        assert(!ml_retry_note_tx_failed(&rt, &ob, 90u, 0u));

        while (ml_transport_prepare_next(&ob, frame, sizeof(frame), &id) != 0u) {
            assert(id == 90u);
            assert(++offered <= 3u); /* must stop after the budget */
            assert(ml_transport_report_tx(&ob, id, false));
            assert(ml_retry_note_tx_failed(&rt, &ob, id, 3u));
        }
        assert(offered == 3u);
        assert(ob.entries[0].state == ML_DELIVERY_FAILED);
        assert(!ml_retry_note_tx_failed(&rt, &ob, 90u, 3u)); /* already FAILED */
    }

    /* Failed and sent attempts share one budget. */
    {
        ml_outbox_t ob;
        ml_retry_state_t rt;
        ml_packet_t p;

        ml_outbox_init(&ob);
        ml_retry_init(&rt);
        memset(&p, 0, sizeof(p));
        p.type = ML_PACKET_DATA;
        p.source = 1u;
        p.destination = 2u;
        p.message_id = 91u;
        assert(ml_outbox_enqueue(&ob, &p));

        assert(ml_retry_note_tx_failed(&rt, &ob, 91u, 3u));      /* attempt 1 */
        assert(ob.entries[0].state == ML_DELIVERY_QUEUED);
        assert(ml_outbox_mark_sent(&ob, 91u));
        assert(ml_retry_note_tx(&rt, &ob, 91u, 0u));             /* attempt 2 */
        assert(ml_retry_poll(&rt, &ob, 2000u, 1000u, 3u) == 1u);  /* requeued */
        assert(ob.entries[0].state == ML_DELIVERY_QUEUED);
        assert(ml_retry_note_tx_failed(&rt, &ob, 91u, 3u));      /* attempt 3 */
        assert(ob.entries[0].state == ML_DELIVERY_FAILED);
    }

    /* A SENT message nobody registered with note_tx is adopted, then times out. */
    {
        ml_outbox_t ob;
        ml_retry_state_t rt;
        ml_packet_t p;

        ml_outbox_init(&ob);
        ml_retry_init(&rt);
        memset(&p, 0, sizeof(p));
        p.type = ML_PACKET_DATA;
        p.source = 1u;
        p.destination = 2u;
        p.message_id = 92u;
        assert(ml_outbox_enqueue(&ob, &p));
        assert(ml_outbox_mark_sent(&ob, 92u)); /* no ml_retry_note_tx() call */

        assert(ml_retry_poll(&rt, &ob, 5000u, 1000u, 2u) == 0u);  /* adopted */
        assert(ob.entries[0].state == ML_DELIVERY_SENT);
        assert(ml_retry_poll(&rt, &ob, 5999u, 1000u, 2u) == 0u);  /* not yet */
        assert(ml_retry_poll(&rt, &ob, 6000u, 1000u, 2u) == 1u);  /* requeued */
        assert(ob.entries[0].state == ML_DELIVERY_QUEUED);

        /* An already-registered message is not re-adopted or double counted. */
        assert(ml_outbox_mark_sent(&ob, 92u));
        assert(ml_retry_note_tx(&rt, &ob, 92u, 7000u));
        assert(ml_retry_poll(&rt, &ob, 7000u, 1000u, 2u) == 0u);
        assert(ml_retry_poll(&rt, &ob, 8999u, 1000u, 2u) == 0u);
        assert(ml_retry_poll(&rt, &ob, 9000u, 1000u, 2u) == 1u);  /* 2nd attempt, 2s */
        assert(ob.entries[0].state == ML_DELIVERY_FAILED);
    }

    puts("mission_link_retry_test: ok");
    return 0;
}
