#include "mission_link_transport.h"
#include "mos_app_messages.h"
#include "mos_app_settings.h"
#include "mos_app_store.h"
#include "mos_test_canvas.h"

#include <assert.h>
#include <string.h>

#define LOCAL 42u
#define PEER 0xA1B2C3D4u

typedef struct {
    rec_canvas_t rec;
    mos_clock_t clock;
    mos_device_info_t dev;
    mos_shell_t shell;
    ml_inbox_t inbox;
    ml_outbox_t outbox;
    ml_service_t service;
    mos_messages_app_t msg;
    mos_app_t msg_app;
    mos_settings_app_t settings;
    mos_app_t settings_app;
    mos_app_t store_app;
    uint64_t t;
} fx_t;

static void fx_init(fx_t *f)
{
    mos_canvas_t c = rec_canvas_make(&f->rec);

    memset(&f->dev, 0, sizeof(f->dev));
    f->dev.radio_state = "ready";
    f->dev.node_id = LOCAL;
    f->dev.battery_percent = -1;
    f->dev.firmware = "test";
    mos_clock_init(&f->clock);
    mos_shell_init(&f->shell, &c, &f->clock, &f->dev, 0);
    ml_inbox_init(&f->inbox);
    ml_outbox_init(&f->outbox);
    assert(ml_service_init(&f->service, LOCAL, 1));
    mos_messages_app_init(&f->msg, &f->msg_app, &f->shell, &f->inbox, &f->outbox, &f->service);
    mos_settings_app_init(&f->settings, &f->settings_app, &f->dev);
    mos_app_store_placeholder_init(&f->store_app);
    assert(mos_shell_add_app(&f->shell, &f->msg_app));
    assert(mos_shell_add_app(&f->shell, &f->settings_app));
    assert(mos_shell_add_app(&f->shell, &f->store_app));
    f->t = 100;
}

/* Deliver a frame from PEER through the real Mission Link receive pipeline. */
static ml_receive_result_t deliver(fx_t *f, uint32_t id, const char *text)
{
    ml_packet_t p;
    uint8_t frame[ML_MAX_FRAME];
    size_t len;

    memset(&p, 0, sizeof(p));
    p.type = ML_PACKET_DATA;
    p.source = PEER;
    p.destination = LOCAL;
    p.message_id = id;
    p.ttl = 4;
    p.payload_length = (uint16_t)strlen(text);
    memcpy(p.payload, text, p.payload_length);
    len = ml_packet_encode(&p, frame, sizeof(frame));
    assert(len > 0);
    return ml_transport_receive(frame, len, LOCAL, &f->inbox, &f->outbox, NULL, 0).data.result;
}

static void gesture(fx_t *f, mos_gesture_t g, int x, int y)
{
    mos_gesture_event_t e;

    e.gesture = g; e.x = (int16_t)x; e.y = (int16_t)y;
    f->t += 50;
    mos_shell_render(&f->shell, f->t); /* the app lays out against the canvas it last drew on */
    mos_shell_on_gesture(&f->shell, &e, f->t);
}

static void open_messages(fx_t *f)
{
    gesture(f, MOS_GESTURE_SWIPE_LEFT, 0, 0);
    gesture(f, MOS_GESTURE_TAP, 30, MOS_HEADER_H + 2); /* first launcher row */
    assert(f->shell.view == MOS_VIEW_APP);
}

#define ROW(n) (MOS_HEADER_H + (n) * MOS_ROW_H + 2)

static void test_receive_read_reply_and_delivery(void)
{
    fx_t f;
    ml_outbox_entry_t out;

    fx_init(&f);
    assert(deliver(&f, 1, "first") == ML_RECEIVE_ACCEPTED);
    assert(deliver(&f, 2, "Meet at the north ridge node at noon please") == ML_RECEIVE_ACCEPTED);
    mos_shell_notify(&f.shell, "New message", f.t);
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "MSG 2")); /* face badge = unread count */

    open_messages(&f);
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "Inbox (2)"));
    /* Newest first: the long message is row 0. */
    assert(strstr(f.rec.texts[1], "Meet at") != NULL);

    gesture(&f, MOS_GESTURE_TAP, 30, ROW(0)); /* open newest */
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "From a1b2c3d4"));
    assert(rec_has(&f.rec, "Meet at the north"));
    assert(f.msg_app.badge(f.msg_app.ctx) == 1); /* opened = read */

    gesture(&f, MOS_GESTURE_TAP, 60, 58); /* reply bar */
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "Reply with"));
    gesture(&f, MOS_GESTURE_TAP, 30, ROW(3)); /* "On my way" */
    assert(f.outbox.count == 1);
    assert(ml_outbox_get(&f.outbox, 0, &out));
    assert(out.state == ML_DELIVERY_QUEUED && out.packet.destination == PEER);
    assert(out.packet.payload_length == 9 && memcmp(out.packet.payload, "On my way", 9) == 0);
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "Queued"));
    assert(rec_has(&f.rec, "From a1b2c3d4")); /* back on the message page */

    /* Outbox page shows distinct delivery states. */
    gesture(&f, MOS_GESTURE_SWIPE_RIGHT, 0, 0); /* -> inbox */
    gesture(&f, MOS_GESTURE_SWIPE_LEFT, 0, 0);  /* -> outbox */
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "queued: On my way"));
    assert(ml_outbox_mark_sent(&f.outbox, out.packet.message_id));
    mos_shell_render(&f.shell, f.t + 2);
    assert(rec_has(&f.rec, "sent: On my way") && !rec_has(&f.rec, "delivered"));

    /* A real ACK frame from the peer marks it delivered. */
    {
        ml_packet_t ack;
        uint8_t frame[ML_MAX_FRAME];
        size_t len;

        memset(&ack, 0, sizeof(ack));
        ack.type = ML_PACKET_ACK; ack.source = PEER; ack.destination = LOCAL;
        ack.message_id = out.packet.message_id; ack.ttl = 4;
        len = ml_packet_encode(&ack, frame, sizeof(frame));
        assert(ml_transport_receive(frame, len, LOCAL, &f.inbox, &f.outbox, NULL, 0).kind ==
               ML_TRANSPORT_RX_ACK_ACCEPTED);
    }
    mos_shell_render(&f.shell, f.t + 3);
    assert(rec_has(&f.rec, "delivered: On my way"));

    gesture(&f, MOS_GESTURE_LONG_PRESS, 30, ROW(0)); /* remove the finished entry */
    assert(f.outbox.count == 0);
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "Nothing queued"));
}

static void test_unfinished_outbox_entry_cannot_be_removed(void)
{
    fx_t f;
    ml_packet_t p;

    fx_init(&f);
    assert(ml_service_make_text(&f.service, PEER, 4, "hold", &p));
    assert(ml_outbox_enqueue(&f.outbox, &p));
    open_messages(&f);
    gesture(&f, MOS_GESTURE_SWIPE_LEFT, 0, 0); /* outbox */
    gesture(&f, MOS_GESTURE_LONG_PRESS, 30, ROW(0));
    assert(f.outbox.count == 1);
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "Not finished yet"));
    assert(ml_outbox_mark_failed(&f.outbox, p.message_id));
    mos_shell_render(&f.shell, f.t + 2);
    assert(rec_has(&f.rec, "FAILED: hold")); /* failure is visibly distinct */
    gesture(&f, MOS_GESTURE_SWIPE_LEFT, 0, 0); /* still on the page: does not close the app */
    assert(f.shell.view == MOS_VIEW_APP);
}

static void test_delete_frees_capacity_and_selection_is_stable(void)
{
    fx_t f;
    uint32_t i;

    fx_init(&f);
    for (i = 1; i <= ML_INBOX_CAPACITY; ++i) {
        assert(deliver(&f, i, "m") == ML_RECEIVE_ACCEPTED);
    }
    /* The inbox does not evict: a full inbox rejects new traffic. */
    assert(deliver(&f, 99, "dropped") == ML_RECEIVE_INBOX_FULL);
    open_messages(&f);
    gesture(&f, MOS_GESTURE_TAP, 30, ROW(0)); /* newest (id 8) */
    gesture(&f, MOS_GESTURE_LONG_PRESS, 0, 0); /* delete it */
    assert(f.inbox.count == ML_INBOX_CAPACITY - 1u);
    assert(f.shell.view == MOS_VIEW_APP);
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "Inbox (7)"));
    assert(deliver(&f, 99, "fits now") == ML_RECEIVE_ACCEPTED);
}

static void test_selection_survives_other_removals(void)
{
    fx_t f;
    ml_packet_t p;

    fx_init(&f);
    deliver(&f, 1, "keep me");
    deliver(&f, 2, "other");
    open_messages(&f);
    gesture(&f, MOS_GESTURE_TAP, 30, ROW(1)); /* "keep me" (older) */
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "keep me"));
    ml_inbox_remove(&f.inbox, PEER, 2); /* something else removes a different entry */
    mos_shell_render(&f.shell, f.t + 2);
    assert(rec_has(&f.rec, "keep me"));
    ml_inbox_remove(&f.inbox, PEER, 1); /* now the selected one vanishes */
    gesture(&f, MOS_GESTURE_TAP, 60, 58);  /* tapping reply must not crash or queue */
    assert(f.outbox.count == 0);
    (void)p;
}

static void test_opaque_payload_is_sanitised_and_back_closes_app(void)
{
    fx_t f;
    ml_packet_t p;
    uint8_t frame[ML_MAX_FRAME];
    size_t len;

    fx_init(&f);
    memset(&p, 0, sizeof(p));
    p.type = ML_PACKET_DATA; p.source = PEER; p.destination = LOCAL; p.message_id = 5; p.ttl = 4;
    p.payload_length = 4;
    p.payload[0] = 'a'; p.payload[1] = 0x07; p.payload[2] = 0xc3; p.payload[3] = 'z';
    len = ml_packet_encode(&p, frame, sizeof(frame));
    assert(ml_transport_receive(frame, len, LOCAL, &f.inbox, &f.outbox, NULL, 0).kind ==
           ML_TRANSPORT_RX_DATA);
    open_messages(&f);
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "a??z"));
    gesture(&f, MOS_GESTURE_SWIPE_RIGHT, 0, 0); /* inbox page leaves swipe-right to the shell */
    assert(f.shell.view == MOS_VIEW_LAUNCHER);
}

static void test_settings_and_store_placeholder(void)
{
    fx_t f;

    fx_init(&f);
    f.dev.rf_enabled = false;
    assert(mos_shell_open(&f.shell, "Settings", 1));
    mos_shell_render(&f.shell, 2);
    assert(rec_has(&f.rec, "Node 0000002a") && rec_has(&f.rec, "TX OFF"));
    assert(rec_has(&f.rec, "Radio ready") && rec_has(&f.rec, "Batt --") && rec_has(&f.rec, "FW test"));
    f.dev.receiving = true; f.dev.rssi = -91; f.dev.snr = 7; f.dev.battery_percent = 80;
    mos_shell_render(&f.shell, 3);
    assert(rec_has(&f.rec, "RSSI -91 SNR 7") && rec_has(&f.rec, "Batt 80%"));

    mos_shell_go_home(&f.shell);
    gesture(&f, MOS_GESTURE_SWIPE_LEFT, 0, 0);
    mos_shell_render(&f.shell, f.t + 1);
    assert(rec_has(&f.rec, "App Store (soon)")); /* visible but marked inactive */
    assert(mos_shell_open(&f.shell, "App Store", f.t + 2));
    mos_shell_render(&f.shell, f.t + 3);
    assert(rec_has(&f.rec, "Not available yet"));
    assert(!rec_has(&f.rec, "[")); /* no button-like controls or fake success states */
}

int main(void)
{
    test_receive_read_reply_and_delivery();
    test_unfinished_outbox_entry_cannot_be_removed();
    test_delete_frees_capacity_and_selection_is_stable();
    test_selection_survives_other_removals();
    test_opaque_payload_is_sanitised_and_back_closes_app();
    test_settings_and_store_placeholder();
    return 0;
}
