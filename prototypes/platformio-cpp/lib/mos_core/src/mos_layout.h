#pragma once
// Shared layout helpers so every app looks consistent on any Canvas size.
#include <stdio.h>
#include <string>
#include <vector>
#include "mos_canvas.h"

namespace mos {
inline int headerH() { return FONT_H + 2; }
inline int rowH() { return FONT_H + 4; }
inline int visibleRows(const Canvas& c) { return (c.height() - headerH()) / rowH(); }
inline int textCols(const Canvas& c) { return c.width() / FONT_W; }

inline void drawHeader(Canvas& c, const char* title) {
  c.fillRect(0, 0, c.width(), headerH() - 1, true);
  c.drawText(2, 1, title, 1, true);
}

// Greedy word wrap into lines of at most `cols` characters (long words split).
std::vector<std::string> wrapText(const char* text, int cols);
}  // namespace mos
