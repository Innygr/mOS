#pragma once
// Reusable, hardware-independent text keyboard for mOS apps.
// Include this header from an app and call begin(), render(), and onTap().
// The host app owns persistence and decides what to do on Confirm/Cancel.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "mos_canvas.h"

namespace mos {

enum class KeyboardResult : uint8_t { None, Changed, Confirm, Cancel };
enum class KeyboardLayout : uint8_t { Letters, Numbers, Symbols };

class TextKeyboard {
 public:
  static constexpr size_t CAPACITY = 96;

  void begin(const char* initial = "", bool uppercase = false) {
    text_[0] = '\0';
    if (initial) {
      strncpy(text_, initial, CAPACITY - 1);
      text_[CAPACITY - 1] = '\0';
    }
    layout_ = KeyboardLayout::Letters;
    uppercase_ = uppercase;
    active_ = true;
  }

  void close() { active_ = false; }
  bool active() const { return active_; }
  const char* value() const { return text_; }
  KeyboardLayout layout() const { return layout_; }
  bool uppercase() const { return uppercase_; }

  // Returns true when the tap belongs to the keyboard, even if it hit empty space.
  // A result other than None describes an action the app should react to.
  bool onTap(int x, int y, KeyboardResult& result) {
    result = KeyboardResult::None;
    if (!active_) return false;
    const int w = canvasWidth_, h = canvasHeight_;
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x >= w || y >= h) return false;

    const int header = h >= 100 ? 22 : 16;
    const int footer = h >= 100 ? 22 : 10;
    if (y < header) return true;  // text preview is not a key
    if (y >= h - footer) {
      const int third = w / 3;
      if (x < third) { active_ = false; result = KeyboardResult::Cancel; }
      else if (x >= third * 2) { active_ = false; result = KeyboardResult::Confirm; }
      else { append(' '); result = KeyboardResult::Changed; }
      return true;
    }

    const int rows = 4;
    const int keyH = (h - header - footer) / rows;
    if (keyH <= 0) return true;
    const int row = (y - header) / keyH;
    if (row < 0 || row >= rows) return true;
    const int colCount = row == 3 ? 4 : (layout_ == KeyboardLayout::Letters && (row == 1 || row == 2) ? 9 : 10);
    const int colW = w / colCount;
    int col = x / (colW > 0 ? colW : 1);
    if (col >= colCount) col = colCount - 1;

    if (layout_ == KeyboardLayout::Letters) {
      static const char* keys[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm", ""};
      if (row == 0 || row == 1) {
        const char* line = keys[row];
        const size_t n = strlen(line);
        if (col >= 0 && static_cast<size_t>(col) < n) append(shift(line[col]));
      } else if (row == 2) {
        // Seven equal key zones: shift, seven letters, backspace.
        if (col == 0) uppercase_ = !uppercase_;
        else if (col == 8) eraseLast();
        else {
          const char c = keys[2][col - 1];
          append(shift(c));
        }
      } else {
        if (col == 0) { layout_ = KeyboardLayout::Numbers; }
        else if (col == 1) append('-');
        else if (col == 2) append('.');
        else if (col == 3) eraseLast();
      }
    } else {
      static const char* nums = "1234567890";
      static const char* syms = "!@#$%^&*()";
      const char* line = layout_ == KeyboardLayout::Numbers ? nums : syms;
      if (row == 0 || row == 1) {
        append(line[col]);
      } else if (row == 2) {
        if (col == 0) layout_ = layout_ == KeyboardLayout::Numbers ? KeyboardLayout::Symbols : KeyboardLayout::Numbers;
        else if (col == 9) eraseLast();
        else append(line[col]);
      } else {
        if (col == 0) layout_ = KeyboardLayout::Letters;
        else if (col == 1) append('_');
        else if (col == 2) append('.');
        else eraseLast();
      }
    }
    result = KeyboardResult::Changed;
    return true;
  }

  void render(Canvas& c, const char* title = "Text") {
    if (!active_) return;
    canvasWidth_ = c.width();
    canvasHeight_ = c.height();
    c.clear();
    const int w = c.width(), h = c.height();
    const int header = h >= 100 ? 22 : 16;
    const int footer = h >= 100 ? 22 : 10;
    c.drawText(2, 1, title);
    char preview[CAPACITY + 1];
    strncpy(preview, text_, sizeof(preview) - 1);
    preview[sizeof(preview) - 1] = '\0';
    const int maxChars = w / FONT_W;
    const size_t len = strlen(preview);
    if (maxChars > 0 && len > static_cast<size_t>(maxChars)) {
      memmove(preview, preview + len - static_cast<size_t>(maxChars), static_cast<size_t>(maxChars) + 1);
      preview[maxChars] = '\0';
    }
    c.drawText(2, header - FONT_H, preview);

    const int keyH = (h - header - footer) / 4;
    if (keyH > 0) {
      for (int row = 0; row < 4; ++row) {
        int cols = row == 3 ? 4 : (layout_ == KeyboardLayout::Letters && (row == 1 || row == 2) ? 9 : 10);
        const int cell = w / cols;
        for (int col = 0; col < cols; ++col) {
          int x = col * cell, y = header + row * keyH;
          c.drawRect(x, y, cell, keyH);
          char label[2] = {' ', '\0'};
          if (layout_ == KeyboardLayout::Letters) {
            static const char* lines[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
            if (row < 2) {
              label[0] = shift(lines[row][col]);
            } else if (row == 2) {
              if (col == 0) label[0] = '^';
              else if (col == 8) label[0] = '<';
              else label[0] = shift("zxcvbnm"[col - 1]);
            } else {
              const char* bottom = "#-.<";
              label[0] = bottom[col];
            }
          } else {
            const char* line = layout_ == KeyboardLayout::Numbers ? "1234567890" : "!@#$%^&*()";
            if (row < 2) label[0] = line[col];
            else if (row == 2) label[0] = col == 0 ? (layout_ == KeyboardLayout::Numbers ? '#' : '1') : (col == 9 ? '<' : line[col]);
            else label[0] = col == 0 ? 'A' : (col == 1 ? '_' : (col == 2 ? '.' : '<'));
          }
          c.drawText(x + (cell > FONT_W ? (cell - FONT_W) / 2 : 0), y + (keyH > FONT_H ? (keyH - FONT_H) / 2 : 0), label);
        }
      }
    }
    const int fy = h - footer;
    c.drawRect(0, fy, w / 3, footer);
    c.drawRect(w / 3, fy, w / 3, footer);
    c.drawRect((w / 3) * 2, fy, w - (w / 3) * 2, footer);
    c.drawText(1, fy + (footer > FONT_H ? (footer - FONT_H) / 2 : 0), "Cancel");
    c.drawText(w / 3 + 1, fy + (footer > FONT_H ? (footer - FONT_H) / 2 : 0), "Space");
    c.drawText((w / 3) * 2 + 1, fy + (footer > FONT_H ? (footer - FONT_H) / 2 : 0), "OK");
  }

 private:
  char text_[CAPACITY] = {0};
  KeyboardLayout layout_ = KeyboardLayout::Letters;
  bool uppercase_ = false;
  bool active_ = false;
  int canvasWidth_ = 0, canvasHeight_ = 0;

  char shift(char c) const {
    if (c >= 'a' && c <= 'z' && uppercase_) return static_cast<char>(c - 'a' + 'A');
    return c;
  }
  void append(char c) {
    const size_t n = strlen(text_);
    if (n + 1 < sizeof(text_)) { text_[n] = c; text_[n + 1] = '\0'; }
  }
  void eraseLast() {
    const size_t n = strlen(text_);
    if (n) text_[n - 1] = '\0';
  }
};

}  // namespace mos
