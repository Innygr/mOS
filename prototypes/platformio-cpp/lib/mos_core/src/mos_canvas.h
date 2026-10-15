#pragma once
// Minimal 1-bit drawing surface. Screens only talk to this, so the same UI runs
// on an SSD1306-class OLED, another Adafruit_GFX display, or a test recorder.
// Text uses a fixed 6x8 cell at scale 1 (the classic Adafruit_GFX font).
namespace mos {
static constexpr int FONT_W = 6;
static constexpr int FONT_H = 8;

class Canvas {
 public:
  virtual ~Canvas() {}
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual void clear() = 0;
  // inverted = dark text on a lit background (caller fills the rect first).
  virtual void drawText(int x, int y, const char* text, int scale = 1, bool inverted = false) = 0;
  virtual void fillRect(int x, int y, int w, int h, bool on) = 0;
  virtual void drawRect(int x, int y, int w, int h) = 0;
  virtual void present() = 0;
};
}  // namespace mos
