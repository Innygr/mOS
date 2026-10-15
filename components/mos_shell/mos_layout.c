#include "mos_layout.h"

#include <string.h>

int mos_visible_rows(const mos_canvas_t *canvas)
{
    return canvas == NULL ? 0 : (canvas->height - MOS_HEADER_H) / MOS_ROW_H;
}

int mos_text_cols(const mos_canvas_t *canvas)
{
    return canvas == NULL ? 0 : canvas->width / MOS_FONT_W;
}

void mos_draw_header(const mos_canvas_t *canvas, const char *title)
{
    canvas->fill_rect(canvas->ctx, 0, 0, canvas->width, MOS_HEADER_H - 1, true);
    canvas->text(canvas->ctx, 2, 1, title, 1, true);
}

size_t mos_wrap_text(const char *text, size_t cols, mos_line_t *lines,
                     size_t max_lines)
{
    size_t n = 0, cur_len = 0;
    char cur[MOS_WRAP_MAX_COLS + 1u];
    const char *p = text;

    if (text == NULL || lines == NULL || max_lines == 0 || cols == 0) {
        return 0;
    }
    if (cols > MOS_WRAP_MAX_COLS) {
        cols = MOS_WRAP_MAX_COLS;
    }
    cur[0] = '\0';
    while (*p != '\0') {
        const char *start;
        size_t wlen;

        while (*p == ' ') {
            ++p;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != ' ') {
            ++p;
        }
        wlen = (size_t)(p - start);
        while (wlen > cols) { /* split overlong words across lines */
            if (cur_len > 0) {
                if (n == max_lines) { return n; }
                memcpy(lines[n].text, cur, cur_len + 1u);
                ++n;
                cur_len = 0;
                cur[0] = '\0';
            }
            if (n == max_lines) { return n; }
            memcpy(lines[n].text, start, cols);
            lines[n].text[cols] = '\0';
            ++n;
            start += cols;
            wlen -= cols;
        }
        if (cur_len == 0) {
            memcpy(cur, start, wlen);
            cur_len = wlen;
            cur[cur_len] = '\0';
        } else if (cur_len + 1u + wlen <= cols) {
            cur[cur_len++] = ' ';
            memcpy(cur + cur_len, start, wlen);
            cur_len += wlen;
            cur[cur_len] = '\0';
        } else {
            if (n == max_lines) { return n; }
            memcpy(lines[n].text, cur, cur_len + 1u);
            ++n;
            memcpy(cur, start, wlen);
            cur_len = wlen;
            cur[cur_len] = '\0';
        }
    }
    if (cur_len > 0 && n < max_lines) {
        memcpy(lines[n].text, cur, cur_len + 1u);
        ++n;
    }
    return n;
}
