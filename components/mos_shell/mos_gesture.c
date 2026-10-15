#include "mos_gesture.h"

#include <string.h>

static int abs_i(int v)
{
    return v < 0 ? -v : v;
}

void mos_gesture_init(mos_gesture_recognizer_t *r)
{
    if (r != NULL) {
        memset(r, 0, sizeof(*r));
    }
}

void mos_gesture_touch_down(mos_gesture_recognizer_t *r, int16_t x, int16_t y,
                            uint64_t now_ms)
{
    if (r == NULL) {
        return;
    }
    r->active = true;
    r->start_x = r->last_x = x;
    r->start_y = r->last_y = y;
    r->down_ms = now_ms;
}

void mos_gesture_touch_move(mos_gesture_recognizer_t *r, int16_t x, int16_t y)
{
    if (r == NULL || !r->active) {
        return;
    }
    r->last_x = x;
    r->last_y = y;
}

mos_gesture_event_t mos_gesture_touch_up(mos_gesture_recognizer_t *r,
                                         uint64_t now_ms)
{
    mos_gesture_event_t ev = { MOS_GESTURE_NONE, 0, 0 };
    int dx, dy, ax, ay;

    if (r == NULL || !r->active) {
        return ev;
    }
    r->active = false;
    ev.x = r->start_x;
    ev.y = r->start_y;
    dx = r->last_x - r->start_x;
    dy = r->last_y - r->start_y;
    ax = abs_i(dx);
    ay = abs_i(dy);

    if (ax <= MOS_TAP_MAX_PX && ay <= MOS_TAP_MAX_PX) {
        ev.gesture = (now_ms - r->down_ms >= MOS_LONG_PRESS_MS)
                         ? MOS_GESTURE_LONG_PRESS : MOS_GESTURE_TAP;
    } else if (ax >= ay && ax >= MOS_SWIPE_MIN_PX) {
        ev.gesture = dx < 0 ? MOS_GESTURE_SWIPE_LEFT : MOS_GESTURE_SWIPE_RIGHT;
    } else if (ay > ax && ay >= MOS_SWIPE_MIN_PX) {
        ev.gesture = dy < 0 ? MOS_GESTURE_SWIPE_UP : MOS_GESTURE_SWIPE_DOWN;
    } /* else: drifted too far for a tap, not far enough for a swipe */
    return ev;
}
