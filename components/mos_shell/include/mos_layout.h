#ifndef MOS_LAYOUT_H
#define MOS_LAYOUT_H

#include <stddef.h>

#include "mos_canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared layout so every app looks consistent on any canvas size. */
#define MOS_HEADER_H (MOS_FONT_H + 2)
#define MOS_ROW_H (MOS_FONT_H + 4)
#define MOS_WRAP_MAX_COLS 40u

int mos_visible_rows(const mos_canvas_t *canvas);
int mos_text_cols(const mos_canvas_t *canvas);
void mos_draw_header(const mos_canvas_t *canvas, const char *title);

typedef struct {
    char text[MOS_WRAP_MAX_COLS + 1u];
} mos_line_t;

/*
 * Greedy word wrap into at most max_lines lines of at most cols characters
 * (cols is clamped to MOS_WRAP_MAX_COLS; overlong words are split). Returns
 * the number of lines written; output beyond max_lines is dropped.
 */
size_t mos_wrap_text(const char *text, size_t cols, mos_line_t *lines,
                     size_t max_lines);

#ifdef __cplusplus
}
#endif

#endif
