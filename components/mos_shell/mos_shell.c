#include "mos_shell.h"

#include <stdio.h>
#include <string.h>

void mos_shell_init(mos_shell_t *shell, const mos_canvas_t *canvas,
                    mos_clock_t *clock, const mos_device_info_t *device,
                    uint64_t now_ms)
{
    memset(shell, 0, sizeof(*shell));
    shell->canvas = *canvas;
    shell->clock = clock;
    shell->device = device;
    shell->view = MOS_VIEW_FACE;
    mos_screen_timeout_init(&shell->screen, (uint32_t)now_ms, MOS_SCREEN_TIMEOUT_DEFAULT_MS);
}

bool mos_shell_add_app(mos_shell_t *shell, const mos_app_t *app)
{
    if (shell == NULL || app == NULL || app->render == NULL || app->name == NULL ||
        shell->app_count >= MOS_SHELL_MAX_APPS) {
        return false;
    }
    shell->apps[shell->app_count++] = app;
    return true;
}

bool mos_shell_open(mos_shell_t *shell, const char *app_name, uint64_t now_ms)
{
    size_t i;

    for (i = 0; shell != NULL && app_name != NULL && i < shell->app_count; ++i) {
        if (strcmp(shell->apps[i]->name, app_name) == 0) {
            shell->running = shell->apps[i];
            shell->view = MOS_VIEW_APP;
            if (shell->running->on_open != NULL) {
                shell->running->on_open(shell->running->ctx, now_ms);
            }
            return true;
        }
    }
    return false;
}

void mos_shell_go_home(mos_shell_t *shell)
{
    shell->view = MOS_VIEW_FACE;
    shell->running = NULL;
}

void mos_shell_notify(mos_shell_t *shell, const char *text, uint64_t now_ms)
{
    mos_screen_timeout_activity(&shell->screen, (uint32_t)now_ms);
    snprintf(shell->toast, sizeof(shell->toast), "%s", text != NULL ? text : "");
    shell->toast_until_ms = now_ms + MOS_TOAST_MS;
}

void mos_shell_set_screen_timeout_ms(mos_shell_t *shell, uint32_t ms)
{
    mos_screen_timeout_set(&shell->screen, ms);
}

bool mos_shell_asleep(const mos_shell_t *shell, uint64_t now_ms)
{
    /* Same rule as mos_screen_timeout_poll(), without mutating state. */
    return !shell->screen.screen_on ||
           (uint32_t)((uint32_t)now_ms - shell->screen.last_activity_ms) >=
               shell->screen.timeout_ms;
}

bool mos_shell_poll_screen_off(mos_shell_t *shell, uint64_t now_ms)
{
    return mos_screen_timeout_poll(&shell->screen, (uint32_t)now_ms);
}

void mos_shell_tick(mos_shell_t *shell, uint64_t now_ms)
{
    size_t i;

    for (i = 0; i < shell->app_count; ++i) {
        if (shell->apps[i]->tick != NULL) {
            shell->apps[i]->tick(shell->apps[i]->ctx, now_ms);
        }
    }
}

void mos_shell_on_gesture(mos_shell_t *shell, const mos_gesture_event_t *ev,
                          uint64_t now_ms)
{
    bool was_asleep;

    if (ev == NULL || ev->gesture == MOS_GESTURE_NONE) {
        return;
    }
    was_asleep = mos_shell_asleep(shell, now_ms);
    mos_screen_timeout_activity(&shell->screen, (uint32_t)now_ms);
    if (was_asleep) {
        mos_shell_go_home(shell); /* wake only */
        return;
    }
    switch (shell->view) {
    case MOS_VIEW_FACE:
        if (ev->gesture == MOS_GESTURE_SWIPE_LEFT) {
            shell->view = MOS_VIEW_LAUNCHER;
            shell->scroll = 0;
        }
        break;
    case MOS_VIEW_LAUNCHER: {
        size_t rows = (size_t)mos_visible_rows(&shell->canvas);

        if (ev->gesture == MOS_GESTURE_SWIPE_RIGHT) {
            mos_shell_go_home(shell);
        } else if (ev->gesture == MOS_GESTURE_SWIPE_UP) {
            if (shell->scroll + rows < shell->app_count) { ++shell->scroll; }
        } else if (ev->gesture == MOS_GESTURE_SWIPE_DOWN) {
            if (shell->scroll > 0) { --shell->scroll; }
        } else if (ev->gesture == MOS_GESTURE_TAP && ev->y >= MOS_HEADER_H) {
            size_t idx = shell->scroll + (size_t)((ev->y - MOS_HEADER_H) / MOS_ROW_H);

            if (idx < shell->app_count) {
                mos_shell_open(shell, shell->apps[idx]->name, now_ms);
            }
        }
        break;
    }
    case MOS_VIEW_APP:
        if (shell->running->on_gesture != NULL &&
            shell->running->on_gesture(shell->running->ctx, ev, now_ms)) {
            break;
        }
        if (ev->gesture == MOS_GESTURE_SWIPE_RIGHT) {
            shell->view = MOS_VIEW_LAUNCHER;
            shell->running = NULL;
        } else if (ev->gesture == MOS_GESTURE_LONG_PRESS) {
            mos_shell_go_home(shell);
        }
        break;
    }
}

static void render_face(mos_shell_t *shell, uint64_t now_ms)
{
    const mos_canvas_t *c = &shell->canvas;
    char buf[24];
    uint8_t h, m, s;
    const int scale = 3;
    int x = 2, y = c->height - MOS_FONT_H - 1;
    const char *radio;
    size_t i;

    if (mos_clock_time_of_day(shell->clock, now_ms, &h, &m, &s)) {
        snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)h, (unsigned)m);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    c->text(c->ctx, (c->width - (int)strlen(buf) * MOS_FONT_W * scale) / 2, 8, buf,
            scale, false);
    if (shell->clock == NULL || !shell->clock->valid) {
        const char *hint = "time not set";

        c->text(c->ctx, (c->width - (int)strlen(hint) * MOS_FONT_W) / 2, 36, hint, 1,
                false);
    }
    for (i = 0; i < shell->app_count; ++i) { /* app badges, e.g. "MSG 2" */
        const mos_app_t *a = shell->apps[i];
        int n = a->badge != NULL ? a->badge(a->ctx) : 0;

        if (n <= 0) {
            continue;
        }
        snprintf(buf, sizeof(buf), "%s %d", a->short_name != NULL ? a->short_name : a->name, n);
        c->text(c->ctx, x, y, buf, 1, false);
        x += ((int)strlen(buf) + 1) * MOS_FONT_W;
    }
    radio = (shell->device != NULL && shell->device->receiving) ? "RX" : "NO RADIO";
    c->text(c->ctx, c->width - (int)strlen(radio) * MOS_FONT_W - 2, y, radio, 1, false);
}

static void render_launcher(mos_shell_t *shell)
{
    const mos_canvas_t *c = &shell->canvas;
    int r;

    mos_draw_header(c, "Apps");
    for (r = 0; r < mos_visible_rows(c); ++r) {
        size_t i = shell->scroll + (size_t)r;
        char buf[48];
        const mos_app_t *a;
        int n;

        if (i >= shell->app_count) {
            break;
        }
        a = shell->apps[i];
        n = a->badge != NULL ? a->badge(a->ctx) : 0;
        if (a->placeholder) {
            snprintf(buf, sizeof(buf), "%s (soon)", a->name);
        } else if (n > 0) {
            snprintf(buf, sizeof(buf), "%s (%d)", a->name, n);
        } else {
            snprintf(buf, sizeof(buf), "%s", a->name);
        }
        c->text(c->ctx, 4, MOS_HEADER_H + r * MOS_ROW_H + 2, buf, 1, false);
    }
}

void mos_shell_render(mos_shell_t *shell, uint64_t now_ms)
{
    const mos_canvas_t *c = &shell->canvas;

    c->clear(c->ctx);
    if (mos_shell_asleep(shell, now_ms)) { /* firmware also powers the panel down */
        c->present(c->ctx);
        return;
    }
    switch (shell->view) {
    case MOS_VIEW_FACE: render_face(shell, now_ms); break;
    case MOS_VIEW_LAUNCHER: render_launcher(shell); break;
    case MOS_VIEW_APP:
        if (shell->running != NULL) {
            shell->running->render(shell->running->ctx, c, now_ms);
        }
        break;
    }
    if (now_ms < shell->toast_until_ms) {
        int w = (int)strlen(shell->toast) * MOS_FONT_W + 4;
        int x = (c->width - w) / 2, y = c->height - MOS_FONT_H - 4;

        c->fill_rect(c->ctx, x, y, w, MOS_FONT_H + 3, false);
        c->rect(c->ctx, x, y, w, MOS_FONT_H + 3);
        c->text(c->ctx, x + 2, y + 2, shell->toast, 1, false);
    }
    c->present(c->ctx);
}
