#ifndef MOS_SHELL_H
#define MOS_SHELL_H

#include <stddef.h>

#include "mos_app.h"
#include "mos_clock.h"
#include "mos_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOS_SHELL_MAX_APPS 8u
#define MOS_SLEEP_AFTER_MS_DEFAULT 15000u
#define MOS_SLEEP_AFTER_MS_MIN 3000u /* never lock the wearer out */
#define MOS_TOAST_MS 1500u
#define MOS_TOAST_MAX 21u

/* Snapshot of device/radio state, filled in by firmware each loop. */
typedef struct {
    const char *radio_state;
    bool rf_enabled;
    bool receiving;
    int16_t rssi;
    int8_t snr;
    uint32_t node_id;
    int8_t battery_percent; /* -1 = unknown */
    const char *firmware;
} mos_device_info_t;

typedef enum { MOS_VIEW_FACE = 0, MOS_VIEW_LAUNCHER, MOS_VIEW_APP } mos_view_t;

typedef struct {
    mos_canvas_t canvas;
    mos_clock_t *clock;
    const mos_device_info_t *device;
    const mos_app_t *apps[MOS_SHELL_MAX_APPS];
    size_t app_count;
    mos_view_t view;
    const mos_app_t *running;
    size_t scroll;
    uint32_t sleep_after_ms;
    uint64_t last_activity_ms;
    uint64_t toast_until_ms;
    char toast[MOS_TOAST_MAX + 1u];
} mos_shell_t;

/*
 * Watch face, launcher and app host:
 *   Face     : swipe left -> Launcher
 *   Launcher : swipe up/down scroll, tap a row -> open app, swipe right -> Face
 *   App      : unhandled swipe right -> Launcher, unhandled long press -> Face
 * The first touch after the screen sleeps only wakes it and is not acted on.
 * The shell keeps pointers to canvas-independent state: clock, device info and
 * the app descriptors must outlive it.
 */
void mos_shell_init(mos_shell_t *shell, const mos_canvas_t *canvas,
                    mos_clock_t *clock, const mos_device_info_t *device,
                    uint64_t now_ms);
bool mos_shell_add_app(mos_shell_t *shell, const mos_app_t *app);
bool mos_shell_open(mos_shell_t *shell, const char *app_name, uint64_t now_ms);
void mos_shell_go_home(mos_shell_t *shell);

void mos_shell_on_gesture(mos_shell_t *shell, const mos_gesture_event_t *ev,
                          uint64_t now_ms);
void mos_shell_tick(mos_shell_t *shell, uint64_t now_ms);
void mos_shell_render(mos_shell_t *shell, uint64_t now_ms);

/* Wakes the screen and shows a short toast (truncated to MOS_TOAST_MAX). */
void mos_shell_notify(mos_shell_t *shell, const char *text, uint64_t now_ms);

void mos_shell_set_sleep_after_ms(mos_shell_t *shell, uint32_t ms);
bool mos_shell_asleep(const mos_shell_t *shell, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
