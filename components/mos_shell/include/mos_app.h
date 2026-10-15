#ifndef MOS_APP_H
#define MOS_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "mos_canvas.h"
#include "mos_gesture.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * An mOS app is a descriptor of callbacks. Apps never touch hardware: they draw
 * on the canvas they are given, receive gestures, and use mos_shell_notify()
 * for toasts. Every callback is optional except render.
 */
typedef struct {
    const char *name;       /* launcher label */
    const char *short_name; /* watch-face badge label, <= 4 chars */
    bool placeholder;       /* shown as "(soon)" in the launcher; app explains itself */
    void *ctx;

    int (*badge)(void *ctx); /* e.g. unread count; > 0 shows on the face */
    void (*on_open)(void *ctx, uint64_t now_ms);
    void (*tick)(void *ctx, uint64_t now_ms); /* every loop, even in background */
    /* Return true if handled. An unhandled swipe-right closes the app and an
     * unhandled long press goes home, so apps only consume what they use. */
    bool (*on_gesture)(void *ctx, const mos_gesture_event_t *ev, uint64_t now_ms);
    void (*render)(void *ctx, const mos_canvas_t *canvas, uint64_t now_ms);
} mos_app_t;

#ifdef __cplusplus
}
#endif

#endif
