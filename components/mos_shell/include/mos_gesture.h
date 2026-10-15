#ifndef MOS_GESTURE_H
#define MOS_GESTURE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOS_TAP_MAX_PX 6
#define MOS_SWIPE_MIN_PX 16
#define MOS_LONG_PRESS_MS 600u

typedef enum {
    MOS_GESTURE_NONE = 0,
    MOS_GESTURE_TAP,
    MOS_GESTURE_LONG_PRESS,
    MOS_GESTURE_SWIPE_LEFT,  /* names describe finger motion: LEFT = toward x=0 */
    MOS_GESTURE_SWIPE_RIGHT,
    MOS_GESTURE_SWIPE_UP,
    MOS_GESTURE_SWIPE_DOWN
} mos_gesture_t;

typedef struct {
    mos_gesture_t gesture;
    int16_t x, y; /* where the touch started */
} mos_gesture_event_t;

typedef struct {
    bool active;
    int16_t start_x, start_y, last_x, last_y;
    uint64_t down_ms;
} mos_gesture_recognizer_t;

/* Turns raw touch samples (down / move / up) into gestures. */
void mos_gesture_init(mos_gesture_recognizer_t *r);
void mos_gesture_touch_down(mos_gesture_recognizer_t *r, int16_t x, int16_t y,
                            uint64_t now_ms);
void mos_gesture_touch_move(mos_gesture_recognizer_t *r, int16_t x, int16_t y);
/* Returns MOS_GESTURE_NONE if no touch was active or it was too ambiguous. */
mos_gesture_event_t mos_gesture_touch_up(mos_gesture_recognizer_t *r,
                                         uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
