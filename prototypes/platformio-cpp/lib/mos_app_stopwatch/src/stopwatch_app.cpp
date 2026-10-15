#include "stopwatch_app.h"
#include "mos_layout.h"
#include <stdio.h>
#include <string.h>

namespace mos {
bool StopwatchApp::onGesture(const GestureEvent& ev, uint32_t nowMs) {
  if (ev.gesture == Gesture::Tap) {
    if (running_) { accumulated_ += nowMs - startedAt_; running_ = false; }
    else { startedAt_ = nowMs; running_ = true; }
    return true;
  }
  if (ev.gesture == Gesture::LongPress) {  // reset; consumed so it does not go home
    running_ = false;
    accumulated_ = 0;
    return true;
  }
  return false;
}

void StopwatchApp::render(Canvas& c, uint32_t nowMs) {
  drawHeader(c, "Stopwatch");
  const uint32_t ms = elapsedMs(nowMs);
  const uint32_t totalS = ms / 1000;
  char buf[24];
  if (totalS >= 3600) snprintf(buf, sizeof(buf), "%u:%02u:%02u", (unsigned)(totalS / 3600),
                               (unsigned)(totalS / 60 % 60), (unsigned)(totalS % 60));
  else snprintf(buf, sizeof(buf), "%02u:%02u.%u", (unsigned)(totalS / 60), (unsigned)(totalS % 60),
                (unsigned)(ms % 1000 / 100));
  const int scale = 2;
  const int w = static_cast<int>(strlen(buf)) * FONT_W * scale;
  c.drawText((c.width() - w) / 2, headerH() + 8, buf, scale);
  const char* hint = running_ ? "tap: stop" : (ms ? "tap: resume  hold: reset" : "tap: start");
  c.drawText(2, c.height() - FONT_H - 1, hint);
}
}  // namespace mos
