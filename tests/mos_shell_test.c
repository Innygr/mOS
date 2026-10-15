#include "mos_shell.h"
#include "mos_test_canvas.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* A trivial app proves the shell works independent of Mission Link. */
typedef struct {
    int badge, ticks, opened, gestures;
    bool consume;
    mos_gesture_t last;
    const char *label;
} dummy_t;

static int d_badge(void *c) { return ((dummy_t *)c)->badge; }
static void d_open(void *c, uint64_t t) { (void)t; ++((dummy_t *)c)->opened; }
static void d_tick(void *c, uint64_t t) { (void)t; ++((dummy_t *)c)->ticks; }
static bool d_gesture(void *c, const mos_gesture_event_t *e, uint64_t t)
{
    dummy_t *d = (dummy_t *)c;
    (void)t;
    ++d->gestures;
    d->last = e->gesture;
    return d->consume;
}
static void d_render(void *c, const mos_canvas_t *cv, uint64_t t)
{
    (void)t;
    cv->text(cv->ctx, 0, 0, ((dummy_t *)c)->label, 1, false);
}

static mos_app_t make_app(dummy_t *d, const char *name)
{
    mos_app_t a;
    memset(&a, 0, sizeof(a));
    d->label = name;
    a.name = name; a.short_name = "DUM"; a.ctx = d;
    a.badge = d_badge; a.on_open = d_open; a.tick = d_tick;
    a.on_gesture = d_gesture; a.render = d_render;
    return a;
}

static mos_gesture_event_t g(mos_gesture_t gg, int x, int y)
{
    mos_gesture_event_t e;
    e.gesture = gg; e.x = (int16_t)x; e.y = (int16_t)y;
    return e;
}

typedef struct {
    rec_canvas_t rec;
    mos_clock_t clock;
    mos_device_info_t dev;
    mos_shell_t shell;
    dummy_t da, db;
    mos_app_t a, b;
} fixture_t;

static void fixture_init(fixture_t *f)
{
    mos_canvas_t c = rec_canvas_make(&f->rec);

    memset(&f->da, 0, sizeof(f->da));
    memset(&f->db, 0, sizeof(f->db));
    memset(&f->dev, 0, sizeof(f->dev));
    mos_clock_init(&f->clock);
    mos_shell_init(&f->shell, &c, &f->clock, &f->dev, 0);
    f->a = make_app(&f->da, "Alpha");
    f->b = make_app(&f->db, "Beta");
    assert(mos_shell_add_app(&f->shell, &f->a));
    assert(mos_shell_add_app(&f->shell, &f->b));
}

static void test_navigation_and_lifecycle(void)
{
    fixture_t f;
    mos_gesture_event_t e;

    fixture_init(&f);
    mos_shell_render(&f.shell, 100);
    assert(rec_has(&f.rec, "--:--"));
    e = g(MOS_GESTURE_SWIPE_LEFT, 0, 0);
    mos_shell_on_gesture(&f.shell, &e, 200);
    assert(f.shell.view == MOS_VIEW_LAUNCHER);
    mos_shell_render(&f.shell, 210);
    assert(rec_has(&f.rec, "Alpha") && rec_has(&f.rec, "Beta"));
    e = g(MOS_GESTURE_TAP, 30, MOS_HEADER_H + MOS_ROW_H + 2); /* second row */
    mos_shell_on_gesture(&f.shell, &e, 300);
    assert(f.shell.view == MOS_VIEW_APP && f.shell.running == &f.b);
    assert(f.db.opened == 1);
    e = g(MOS_GESTURE_SWIPE_RIGHT, 0, 0); /* unhandled -> launcher */
    mos_shell_on_gesture(&f.shell, &e, 400);
    assert(f.shell.view == MOS_VIEW_LAUNCHER && f.shell.running == NULL);
    e = g(MOS_GESTURE_TAP, 30, MOS_HEADER_H + 2);
    mos_shell_on_gesture(&f.shell, &e, 500);
    e = g(MOS_GESTURE_LONG_PRESS, 0, 0); /* unhandled -> home */
    mos_shell_on_gesture(&f.shell, &e, 600);
    assert(f.shell.view == MOS_VIEW_FACE && f.shell.running == NULL);
}

static void test_app_consumes_gestures(void)
{
    fixture_t f;
    mos_gesture_event_t e = g(MOS_GESTURE_SWIPE_RIGHT, 0, 0);

    fixture_init(&f);
    assert(mos_shell_open(&f.shell, "Alpha", 0));
    f.da.consume = true;
    mos_shell_on_gesture(&f.shell, &e, 10);
    assert(f.shell.view == MOS_VIEW_APP && f.da.gestures == 1);
    assert(!mos_shell_open(&f.shell, "Nope", 0));
    assert(!mos_shell_open(&f.shell, NULL, 0));
}

static void test_ticks_badges_and_toast(void)
{
    fixture_t f;

    fixture_init(&f);
    mos_shell_tick(&f.shell, 1);
    mos_shell_tick(&f.shell, 2);
    assert(f.da.ticks == 2 && f.db.ticks == 2); /* not foreground, still ticks */
    f.da.badge = 3;
    mos_shell_notify(&f.shell, "Hello", 100);
    mos_shell_render(&f.shell, 110);
    assert(rec_has(&f.rec, "DUM 3") && rec_has(&f.rec, "Hello"));
    mos_shell_render(&f.shell, 100 + MOS_TOAST_MS + 1);
    assert(!rec_has(&f.rec, "Hello"));
    f.dev.receiving = true;
    mos_shell_render(&f.shell, 2000);
    assert(rec_has(&f.rec, "RX"));
    mos_shell_notify(&f.shell, "a very long toast message that overflows", 3000);
    assert(strlen(f.shell.toast) == MOS_TOAST_MAX); /* truncated, never overflows */
}

static void test_launcher_scroll_limit_and_placeholder(void)
{
    fixture_t f;
    dummy_t d[MOS_SHELL_MAX_APPS];
    mos_app_t apps[MOS_SHELL_MAX_APPS];
    mos_gesture_event_t up = g(MOS_GESTURE_SWIPE_UP, 0, 0);
    mos_gesture_event_t left = g(MOS_GESTURE_SWIPE_LEFT, 0, 0);
    size_t i;

    fixture_init(&f);
    {
        static const char *const names[] = { "C", "D", "E", "H", "I", "J" };

        for (i = 0; i < MOS_SHELL_MAX_APPS - 2u; ++i) {
            memset(&d[i], 0, sizeof(d[i]));
            apps[i] = make_app(&d[i], names[i]);
        }
        apps[3].placeholder = true; /* "H" */
        for (i = 0; i < MOS_SHELL_MAX_APPS - 2u; ++i) {
            assert(mos_shell_add_app(&f.shell, &apps[i]));
        }
    }
    assert(f.shell.app_count == MOS_SHELL_MAX_APPS);
    assert(!mos_shell_add_app(&f.shell, &apps[0])); /* full */
    mos_shell_on_gesture(&f.shell, &left, 10);
    for (i = 0; i < 10; ++i) { mos_shell_on_gesture(&f.shell, &up, 20); } /* clamps */
    mos_shell_render(&f.shell, 30);
    assert(!rec_has(&f.rec, "Alpha"));
    assert(rec_has(&f.rec, "J"));
    assert(rec_has(&f.rec, "H (soon)"));
    {
        mos_app_t bad;
        memset(&bad, 0, sizeof(bad));
        assert(!mos_shell_add_app(&f.shell, &bad)); /* no render/name */
    }
}

static void test_sleep_and_wake(void)
{
    fixture_t f;
    mos_gesture_event_t e = g(MOS_GESTURE_SWIPE_LEFT, 0, 0);
    uint64_t later = 100u + MOS_SCREEN_TIMEOUT_DEFAULT_MS + 1u;

    fixture_init(&f);
    mos_shell_on_gesture(&f.shell, &e, 100);
    assert(f.shell.view == MOS_VIEW_LAUNCHER);
    assert(!mos_shell_asleep(&f.shell, 100u + MOS_SCREEN_TIMEOUT_DEFAULT_MS - 1u));
    assert(mos_shell_asleep(&f.shell, later));
    mos_shell_render(&f.shell, later);
    assert(f.rec.count == 0 && f.rec.presents > 0);
    /* The driver is told to power down exactly once. */
    assert(mos_shell_poll_screen_off(&f.shell, later));
    assert(!mos_shell_poll_screen_off(&f.shell, later + 1000u));
    assert(mos_shell_asleep(&f.shell, later + 1000u));
    mos_shell_on_gesture(&f.shell, &e, later + 2000u); /* wakes, goes home, does not navigate */
    assert(f.shell.view == MOS_VIEW_FACE && !mos_shell_asleep(&f.shell, later + 2000u));
    mos_shell_poll_screen_off(&f.shell, later + 2001u);
    assert(!mos_shell_asleep(&f.shell, later + 2001u));
    /* A notification also wakes the screen. */
    assert(mos_shell_poll_screen_off(&f.shell, later + 2000u + MOS_SCREEN_TIMEOUT_DEFAULT_MS));
    mos_shell_notify(&f.shell, "x", later + 40000u);
    assert(!mos_shell_asleep(&f.shell, later + 40000u));
    /* Configurable timeout is clamped by the shared policy. */
    mos_shell_set_screen_timeout_ms(&f.shell, 1);
    assert(f.shell.screen.timeout_ms == MOS_SCREEN_TIMEOUT_MIN_MS);
    mos_shell_set_screen_timeout_ms(&f.shell, 60000);
    assert(!mos_shell_asleep(&f.shell, later + 40000u + 59000u));
    assert(mos_shell_asleep(&f.shell, later + 40000u + 60000u));
}

int main(void)
{
    test_navigation_and_lifecycle();
    test_app_consumes_gestures();
    test_ticks_badges_and_toast();
    test_launcher_scroll_limit_and_placeholder();
    test_sleep_and_wake();
    return 0;
}
