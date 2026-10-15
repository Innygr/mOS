#include "mos_app_settings.h"

#include <stdio.h>
#include <string.h>

static void render(void *ctx, const mos_canvas_t *c, uint64_t now_ms)
{
    const mos_device_info_t *d = ((mos_settings_app_t *)ctx)->device;
    char buf[40];
    int y = MOS_HEADER_H + 1;

    (void)now_ms;
    mos_draw_header(c, "Settings");
    if (d == NULL) {
        return;
    }
#define LINE(...) do { snprintf(buf, sizeof(buf), __VA_ARGS__); \
                       c->text(c->ctx, 0, y, buf, 1, false); y += MOS_FONT_H; } while (0)
    LINE("Node %08x", (unsigned)d->node_id);
    LINE("Radio %s", d->radio_state != NULL ? d->radio_state : "unknown");
    LINE("TX %s", d->rf_enabled ? "ON" : "OFF");
    if (d->receiving) {
        LINE("RSSI %d SNR %d", (int)d->rssi, (int)d->snr);
    } else {
        LINE("RSSI --");
    }
    if (d->battery_percent >= 0) {
        LINE("Batt %d%%", (int)d->battery_percent);
    } else {
        LINE("Batt --");
    }
    LINE("FW %s", d->firmware != NULL ? d->firmware : "?");
#undef LINE
}

void mos_settings_app_init(mos_settings_app_t *app, mos_app_t *app_out,
                           const mos_device_info_t *device)
{
    app->device = device;
    memset(app_out, 0, sizeof(*app_out));
    app_out->name = "Settings";
    app_out->short_name = "SET";
    app_out->ctx = app;
    app_out->render = render;
}
