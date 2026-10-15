#ifndef MOS_CANVAS_H
#define MOS_CANVAS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed 6x8 text cell at scale 1 (the classic 5x7 GFX font plus spacing). */
#define MOS_FONT_W 6
#define MOS_FONT_H 8

/*
 * Minimal 1-bit drawing surface. The shell and apps draw only through this, so
 * the same UI runs on any panel once a driver fills it in (the display model
 * is still an open hardware decision) and on a recording canvas in host tests.
 * inverted = dark text on a lit background; the caller fills the rect first.
 */
typedef struct {
    void *ctx;
    int width;
    int height;
    void (*clear)(void *ctx);
    void (*text)(void *ctx, int x, int y, const char *s, int scale, bool inverted);
    void (*fill_rect)(void *ctx, int x, int y, int w, int h, bool on);
    void (*rect)(void *ctx, int x, int y, int w, int h);
    void (*present)(void *ctx);
} mos_canvas_t;

#ifdef __cplusplus
}
#endif

#endif
