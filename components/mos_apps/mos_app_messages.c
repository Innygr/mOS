#include "mos_app_messages.h"

#include <stdio.h>
#include <string.h>

#define PHRASE_COUNT 6u
static const char *const PHRASES[PHRASE_COUNT] = {
    "OK", "Yes", "No", "On my way", "Need help", "Call me"
};
#define MAX_TEXT_LINES 16u

/* Payload bytes are opaque; show only printable ASCII. */
static bool read_display_text(const ml_packet_t *p, char *out, size_t cap)
{
    size_t i, n = p->payload_length;

    if (cap == 0 || n + 1u > cap) {
        return false;
    }
    for (i = 0; i < n; ++i) {
        uint8_t c = p->payload[i];

        out[i] = (c >= 0x20u && c < 0x7fu) ? (char)c : '?';
    }
    out[n] = '\0';
    return true;
}

static int unread_count(const ml_inbox_t *inbox)
{
    size_t i;
    int n = 0;

    for (i = 0; i < inbox->count; ++i) {
        n += inbox->entries[i].unread ? 1 : 0;
    }
    return n;
}

static void go(mos_messages_app_t *a, mos_messages_page_t page)
{
    a->page = page;
    a->scroll = 0;
}

static bool find_selected(const mos_messages_app_t *a, ml_inbox_entry_t *out)
{
    size_t i;

    for (i = 0; a->have_selection && i < a->inbox->count; ++i) {
        const ml_packet_t *p = &a->inbox->entries[i].packet;

        if (p->source == a->sel_source && p->message_id == a->sel_id) {
            *out = a->inbox->entries[i];
            return true;
        }
    }
    return false;
}

static int badge(void *ctx)
{
    return unread_count(((mos_messages_app_t *)ctx)->inbox);
}

static void on_open(void *ctx, uint64_t now_ms)
{
    (void)now_ms;
    go((mos_messages_app_t *)ctx, MOS_MSG_PAGE_INBOX);
}

/* i-th newest inbox/outbox position (0 = newest); false if out of range. */
static bool newest_index(size_t count, size_t i, size_t *idx)
{
    if (i >= count) {
        return false;
    }
    *idx = count - 1u - i;
    return true;
}

static bool scroll_by(mos_messages_app_t *a, mos_gesture_t g, size_t total, size_t window)
{
    if (g == MOS_GESTURE_SWIPE_UP) {
        if (a->scroll + window < total) { ++a->scroll; }
        return true;
    }
    if (g == MOS_GESTURE_SWIPE_DOWN) {
        if (a->scroll > 0) { --a->scroll; }
        return true;
    }
    return false;
}

static size_t message_text_rows(const mos_messages_app_t *a)
{
    int rows = (a->canvas_height - MOS_HEADER_H - MOS_ROW_H) / MOS_FONT_H;

    return rows > 0 ? (size_t)rows : 0u;
}

static bool on_gesture(void *ctx, const mos_gesture_event_t *ev, uint64_t now_ms)
{
    mos_messages_app_t *a = (mos_messages_app_t *)ctx;
    ml_inbox_entry_t sel;
    size_t rows = (size_t)(a->rows > 0 ? a->rows : 0);

    if (a->canvas_height == 0) {
        return false; /* nothing drawn yet, so no layout to hit-test */
    }
    switch (a->page) {
    case MOS_MSG_PAGE_INBOX: {
        size_t idx;

        if (scroll_by(a, ev->gesture, a->inbox->count, rows)) { return true; }
        if (ev->gesture == MOS_GESTURE_SWIPE_LEFT) { go(a, MOS_MSG_PAGE_OUTBOX); return true; }
        if (ev->gesture == MOS_GESTURE_TAP && ev->y >= MOS_HEADER_H) {
            size_t row = a->scroll + (size_t)((ev->y - MOS_HEADER_H) / MOS_ROW_H);

            if (newest_index(a->inbox->count, row, &idx)) {
                const ml_packet_t *p = &a->inbox->entries[idx].packet;

                a->have_selection = true;
                a->sel_source = p->source;
                a->sel_id = p->message_id;
                ml_inbox_mark_read(a->inbox, p->source, p->message_id);
                go(a, MOS_MSG_PAGE_MESSAGE);
            }
            return true;
        }
        return false; /* swipe right etc.: the shell closes the app */
    }
    case MOS_MSG_PAGE_MESSAGE:
        if (ev->gesture == MOS_GESTURE_SWIPE_RIGHT) { go(a, MOS_MSG_PAGE_INBOX); return true; }
        if (!find_selected(a, &sel)) { go(a, MOS_MSG_PAGE_INBOX); return true; }
        if (ev->gesture == MOS_GESTURE_SWIPE_UP || ev->gesture == MOS_GESTURE_SWIPE_DOWN) {
            char text[ML_MAX_PAYLOAD + 1u];
            mos_line_t lines[MAX_TEXT_LINES];
            size_t n = 0;

            if (read_display_text(&sel.packet, text, sizeof(text))) {
                n = mos_wrap_text(text, (size_t)a->cols, lines, MAX_TEXT_LINES);
            }
            return scroll_by(a, ev->gesture, n, message_text_rows(a));
        }
        if (ev->gesture == MOS_GESTURE_TAP) {
            if (ev->y >= a->canvas_height - MOS_ROW_H) { go(a, MOS_MSG_PAGE_REPLY); }
            return true;
        }
        if (ev->gesture == MOS_GESTURE_LONG_PRESS) {
            ml_inbox_remove(a->inbox, a->sel_source, a->sel_id);
            a->have_selection = false;
            go(a, MOS_MSG_PAGE_INBOX);
            mos_shell_notify(a->shell, "Deleted", now_ms);
            return true;
        }
        return false;
    case MOS_MSG_PAGE_REPLY:
        if (ev->gesture == MOS_GESTURE_SWIPE_RIGHT) { go(a, MOS_MSG_PAGE_MESSAGE); return true; }
        if (scroll_by(a, ev->gesture, PHRASE_COUNT, rows)) { return true; }
        if (ev->gesture == MOS_GESTURE_TAP && ev->y >= MOS_HEADER_H) {
            size_t row = (size_t)((ev->y - MOS_HEADER_H) / MOS_ROW_H);
            size_t phrase = a->scroll + row;

            if (row < rows && phrase < PHRASE_COUNT && find_selected(a, &sel)) {
                ml_packet_t reply;
                bool ok = ml_service_make_text(a->service, sel.packet.source,
                                               MOS_MSG_REPLY_TTL, PHRASES[phrase], &reply) &&
                          ml_outbox_enqueue(a->outbox, &reply);

                mos_shell_notify(a->shell, ok ? "Queued" : "Outbox full", now_ms);
                go(a, MOS_MSG_PAGE_MESSAGE);
            }
            return true;
        }
        return false;
    case MOS_MSG_PAGE_OUTBOX:
        if (ev->gesture == MOS_GESTURE_SWIPE_RIGHT) { go(a, MOS_MSG_PAGE_INBOX); return true; }
        if (scroll_by(a, ev->gesture, a->outbox->count, rows)) { return true; }
        if (ev->gesture == MOS_GESTURE_LONG_PRESS && ev->y >= MOS_HEADER_H) {
            size_t row = a->scroll + (size_t)((ev->y - MOS_HEADER_H) / MOS_ROW_H);
            size_t idx;

            if (newest_index(a->outbox->count, row, &idx)) {
                bool removed = ml_outbox_remove_finished(
                    a->outbox, a->outbox->entries[idx].packet.message_id);

                mos_shell_notify(a->shell, removed ? "Removed" : "Not finished yet", now_ms);
            }
            return true;
        }
        return true; /* taps do nothing here; keep swipe-left etc. from closing the app */
    }
    return false;
}

static const char *state_label(ml_delivery_state_t s)
{
    switch (s) {
    case ML_DELIVERY_QUEUED: return "queued";
    case ML_DELIVERY_SENT: return "sent";
    case ML_DELIVERY_DELIVERED: return "delivered";
    case ML_DELIVERY_FAILED: return "FAILED";
    }
    return "?";
}

static void render_inbox(mos_messages_app_t *a, const mos_canvas_t *c)
{
    char buf[ML_MAX_PAYLOAD + 32u];
    char text[ML_MAX_PAYLOAD + 1u];
    int r;

    snprintf(buf, sizeof(buf), "Inbox (%u)", (unsigned)a->inbox->count);
    mos_draw_header(c, buf);
    if (a->inbox->count == 0) {
        c->text(c->ctx, 2, MOS_HEADER_H + 8, "No messages", 1, false);
        return;
    }
    for (r = 0; r < a->rows; ++r) {
        size_t idx;
        const ml_inbox_entry_t *e;

        if (!newest_index(a->inbox->count, a->scroll + (size_t)r, &idx)) {
            break;
        }
        e = &a->inbox->entries[idx];
        if (!read_display_text(&e->packet, text, sizeof(text))) {
            snprintf(text, sizeof(text), "(unreadable)");
        }
        snprintf(buf, sizeof(buf), "%c%04x %s", e->unread ? '*' : ' ',
                 (unsigned)(e->packet.source & 0xffffu), text);
        if (a->cols > 0 && (size_t)a->cols < sizeof(buf)) { buf[a->cols] = '\0'; }
        c->text(c->ctx, 0, MOS_HEADER_H + r * MOS_ROW_H + 2, buf, 1, false);
    }
}

static void render_message(mos_messages_app_t *a, const mos_canvas_t *c)
{
    ml_inbox_entry_t sel;
    char text[ML_MAX_PAYLOAD + 1u];
    char buf[32];
    mos_line_t lines[MAX_TEXT_LINES];
    size_t n = 0, r, rows = message_text_rows(a);
    int by;

    if (!find_selected(a, &sel)) {
        mos_draw_header(c, "Message");
        return;
    }
    snprintf(buf, sizeof(buf), "From %08x", (unsigned)sel.packet.source);
    mos_draw_header(c, buf);
    if (read_display_text(&sel.packet, text, sizeof(text))) {
        n = mos_wrap_text(text, (size_t)a->cols, lines, MAX_TEXT_LINES);
    }
    for (r = 0; r < rows && a->scroll + r < n; ++r) {
        c->text(c->ctx, 0, MOS_HEADER_H + 1 + (int)r * MOS_FONT_H,
                lines[a->scroll + r].text, 1, false);
    }
    by = c->height - MOS_ROW_H;
    c->rect(c->ctx, 0, by, c->width, MOS_ROW_H);
    c->text(c->ctx, 2, by + 2, "[ Reply ]", 1, false);
}

static void render_reply(mos_messages_app_t *a, const mos_canvas_t *c)
{
    int r;

    mos_draw_header(c, "Reply with");
    for (r = 0; r < a->rows; ++r) {
        size_t phrase = a->scroll + (size_t)r;

        if (phrase >= PHRASE_COUNT) {
            break;
        }
        c->text(c->ctx, 4, MOS_HEADER_H + r * MOS_ROW_H + 2, PHRASES[phrase], 1, false);
    }
}

static void render_outbox(mos_messages_app_t *a, const mos_canvas_t *c)
{
    char buf[ML_MAX_PAYLOAD + 32u];
    char text[ML_MAX_PAYLOAD + 1u];
    int r;

    snprintf(buf, sizeof(buf), "Outbox (%u)", (unsigned)a->outbox->count);
    mos_draw_header(c, buf);
    if (a->outbox->count == 0) {
        c->text(c->ctx, 2, MOS_HEADER_H + 8, "Nothing queued", 1, false);
        return;
    }
    for (r = 0; r < a->rows; ++r) {
        size_t idx;
        const ml_outbox_entry_t *e;

        if (!newest_index(a->outbox->count, a->scroll + (size_t)r, &idx)) {
            break;
        }
        e = &a->outbox->entries[idx];
        if (!read_display_text(&e->packet, text, sizeof(text))) {
            snprintf(text, sizeof(text), "(unreadable)");
        }
        snprintf(buf, sizeof(buf), "%s: %s", state_label(e->state), text);
        if (a->cols > 0 && (size_t)a->cols < sizeof(buf)) { buf[a->cols] = '\0'; }
        c->text(c->ctx, 0, MOS_HEADER_H + r * MOS_ROW_H + 2, buf, 1, false);
    }
}

static void render(void *ctx, const mos_canvas_t *c, uint64_t now_ms)
{
    mos_messages_app_t *a = (mos_messages_app_t *)ctx;

    (void)now_ms;
    a->rows = mos_visible_rows(c);
    a->cols = mos_text_cols(c);
    a->canvas_height = c->height;
    switch (a->page) {
    case MOS_MSG_PAGE_INBOX: render_inbox(a, c); break;
    case MOS_MSG_PAGE_MESSAGE: render_message(a, c); break;
    case MOS_MSG_PAGE_REPLY: render_reply(a, c); break;
    case MOS_MSG_PAGE_OUTBOX: render_outbox(a, c); break;
    }
}

void mos_messages_app_init(mos_messages_app_t *app, mos_app_t *app_out,
                           mos_shell_t *shell, ml_inbox_t *inbox,
                           ml_outbox_t *outbox, ml_service_t *service)
{
    memset(app, 0, sizeof(*app));
    app->shell = shell;
    app->inbox = inbox;
    app->outbox = outbox;
    app->service = service;
    memset(app_out, 0, sizeof(*app_out));
    app_out->name = "Messages";
    app_out->short_name = "MSG";
    app_out->ctx = app;
    app_out->badge = badge;
    app_out->on_open = on_open;
    app_out->on_gesture = on_gesture;
    app_out->render = render;
}
