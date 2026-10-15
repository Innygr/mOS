#include "mos_clock.h"
#include "mos_gesture.h"
#include "mos_layout.h"

#include <assert.h>
#include <string.h>

static mos_gesture_t swipe(int sx, int sy, int ex, int ey, uint64_t ms)
{
    mos_gesture_recognizer_t r;

    mos_gesture_init(&r);
    mos_gesture_touch_down(&r, (int16_t)sx, (int16_t)sy, 0);
    mos_gesture_touch_move(&r, (int16_t)((sx + ex) / 2), (int16_t)((sy + ey) / 2));
    mos_gesture_touch_move(&r, (int16_t)ex, (int16_t)ey);
    return mos_gesture_touch_up(&r, ms).gesture;
}

static void test_gestures(void)
{
    mos_gesture_recognizer_t r;
    mos_gesture_event_t ev;

    assert(swipe(50, 30, 52, 31, 100) == MOS_GESTURE_TAP);
    assert(swipe(50, 30, 50, 30, 700) == MOS_GESTURE_LONG_PRESS);
    assert(swipe(100, 30, 40, 32, 200) == MOS_GESTURE_SWIPE_LEFT);
    assert(swipe(20, 30, 90, 28, 200) == MOS_GESTURE_SWIPE_RIGHT);
    assert(swipe(60, 55, 62, 10, 200) == MOS_GESTURE_SWIPE_UP);
    assert(swipe(60, 10, 58, 55, 200) == MOS_GESTURE_SWIPE_DOWN);
    assert(swipe(60, 30, 60, 42, 100) == MOS_GESTURE_NONE); /* 12 px: neither */

    mos_gesture_init(&r);
    assert(mos_gesture_touch_up(&r, 0).gesture == MOS_GESTURE_NONE); /* up without down */
    mos_gesture_touch_down(&r, 7, 9, 0);
    ev = mos_gesture_touch_up(&r, 10);
    assert(ev.gesture == MOS_GESTURE_TAP && ev.x == 7 && ev.y == 9);
    assert(!r.active);
    assert(mos_gesture_touch_up(&r, 20).gesture == MOS_GESTURE_NONE); /* consumed once */
    mos_gesture_touch_move(NULL, 1, 1); /* NULL-safe */
}

static void test_clock(void)
{
    mos_clock_t c;
    uint8_t h, m, s;

    mos_clock_init(&c);
    assert(!mos_clock_time_of_day(&c, 0, &h, &m, &s));
    mos_clock_set_epoch(&c, 86400u * 3u + 3600u * 23u + 59u * 60u + 50u, 1000);
    assert(mos_clock_time_of_day(&c, 1000, &h, &m, &s));
    assert(h == 23 && m == 59 && s == 50);
    mos_clock_time_of_day(&c, 11000, &h, &m, &s); /* +10 s wraps past midnight */
    assert(h == 0 && m == 0);
    mos_clock_set_utc_offset_min(&c, -480);
    mos_clock_time_of_day(&c, 1000, &h, &m, &s);
    assert(h == 15 && m == 59);
    mos_clock_set_epoch(&c, 100, 0); /* small epoch + negative offset wraps, not negative */
    mos_clock_time_of_day(&c, 0, &h, &m, &s);
    assert(h == 16 && m == 1);
    assert(!mos_clock_time_of_day(NULL, 0, &h, &m, &s));
}

static void test_wrap(void)
{
    mos_line_t l[8];
    size_t n = mos_wrap_text("the quick brown fox", 9, l, 8);

    assert(n == 2);
    assert(strcmp(l[0].text, "the quick") == 0);
    assert(strcmp(l[1].text, "brown fox") == 0);

    n = mos_wrap_text("abcdefghijkl", 5, l, 8);
    assert(n == 3);
    assert(strcmp(l[0].text, "abcde") == 0);
    assert(strcmp(l[1].text, "fghij") == 0);
    assert(strcmp(l[2].text, "kl") == 0);

    assert(mos_wrap_text("   ", 5, l, 8) == 0);
    assert(mos_wrap_text(NULL, 5, l, 8) == 0);
    assert(mos_wrap_text("x", 0, l, 8) == 0);
    n = mos_wrap_text("a b c d e f g", 1, l, 3); /* output is bounded */
    assert(n == 3);
    n = mos_wrap_text("one two three four five six", 100, l, 8); /* cols clamped to 40 */
    assert(n == 1 && strlen(l[0].text) <= MOS_WRAP_MAX_COLS);
}

int main(void)
{
    test_gestures();
    test_clock();
    test_wrap();
    return 0;
}
