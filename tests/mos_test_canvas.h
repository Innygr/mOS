#ifndef MOS_TEST_CANVAS_H
#define MOS_TEST_CANVAS_H

/* Recording canvas for host tests. */
#include <string.h>

#include "mos_canvas.h"

#define REC_MAX 96
typedef struct {
    char texts[REC_MAX][64];
    int count;
    int presents;
} rec_canvas_t;

static void rec_clear(void *ctx) { ((rec_canvas_t *)ctx)->count = 0; }
static void rec_text(void *ctx, int x, int y, const char *s, int scale, bool inv)
{
    rec_canvas_t *r = (rec_canvas_t *)ctx;
    (void)x; (void)y; (void)scale; (void)inv;
    if (r->count < REC_MAX) {
        strncpy(r->texts[r->count], s, 63);
        r->texts[r->count][63] = '\0';
        ++r->count;
    }
}
static void rec_fill(void *ctx, int x, int y, int w, int h, bool on)
{
    (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)on;
}
static void rec_rect(void *ctx, int x, int y, int w, int h)
{
    (void)ctx; (void)x; (void)y; (void)w; (void)h;
}
static void rec_present(void *ctx) { ++((rec_canvas_t *)ctx)->presents; }

static mos_canvas_t rec_canvas_make(rec_canvas_t *r)
{
    mos_canvas_t c;
    memset(r, 0, sizeof(*r));
    c.ctx = r; c.width = 128; c.height = 64;
    c.clear = rec_clear; c.text = rec_text; c.fill_rect = rec_fill;
    c.rect = rec_rect; c.present = rec_present;
    return c;
}

/* True if any drawn text contains `needle`. */
static int rec_has(const rec_canvas_t *r, const char *needle)
{
    int i;
    for (i = 0; i < r->count; ++i) {
        if (strstr(r->texts[i], needle) != NULL) { return 1; }
    }
    return 0;
}

#endif
