#include "mos_app_store.h"

#include <string.h>

#define STORE_LINES 8u

static void render(void *ctx, const mos_canvas_t *c, uint64_t now_ms)
{
    mos_line_t lines[STORE_LINES];
    size_t i, n;

    (void)ctx;
    (void)now_ms;
    mos_draw_header(c, "App Store");
    n = mos_wrap_text("Not available yet. Installing apps needs an app API, "
                      "signed packages and rollback first.",
                      (size_t)mos_text_cols(c), lines, STORE_LINES);
    for (i = 0; i < n; ++i) {
        c->text(c->ctx, 0, MOS_HEADER_H + 2 + (int)i * MOS_FONT_H, lines[i].text, 1, false);
    }
}

void mos_app_store_placeholder_init(mos_app_t *app_out)
{
    memset(app_out, 0, sizeof(*app_out));
    app_out->name = "App Store";
    app_out->short_name = "STO";
    app_out->placeholder = true;
    app_out->render = render;
}
