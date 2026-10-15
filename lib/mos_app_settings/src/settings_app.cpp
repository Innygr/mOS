#include "settings_app.h"
#include "mos_layout.h"
#include <stdio.h>
#include <stdlib.h>

namespace mos {
static const uint32_t TIMEOUTS_MS[] = {10000, 15000, 30000, 60000};
static constexpr int16_t MAX_OFFSET_MIN = 14 * 60;  // UTC+14 is the largest real offset
static constexpr int16_t MIN_OFFSET_MIN = -12 * 60;

void SettingsApp::onStart() {
  if (sys().store) {
    uint32_t v;
    if (sys().store->getU32(KEY_UTC_OFFSET, v)) {
      const int16_t off = static_cast<int16_t>(v);  // stored as the 16-bit two's complement
      if (off >= MIN_OFFSET_MIN && off <= MAX_OFFSET_MIN) offsetMin_ = off;
    }
    if (sys().store->getU32(KEY_SLEEP_MS, v)) {
      for (uint32_t t : TIMEOUTS_MS) if (t == v) sleepMs_ = v;  // ignore corrupt values
    }
  }
  apply();
}

void SettingsApp::apply() {
  if (sys().clock) sys().clock->setUtcOffsetMinutes(offsetMin_);
  if (sys().power) sys().power->setSleepAfterMs(sleepMs_);
}

void SettingsApp::save() {
  if (!sys().store) return;
  sys().store->putU32(KEY_UTC_OFFSET, static_cast<uint16_t>(offsetMin_));
  sys().store->putU32(KEY_SLEEP_MS, sleepMs_);
}

bool SettingsApp::onGesture(const GestureEvent& ev, uint32_t) {
  if (ev.gesture != Gesture::Tap) return false;
  const int row = (ev.y - headerH()) / rowH();
  if (ev.y < headerH()) return true;
  if (row == 0) {
    const int next = offsetMin_ + (ev.x < 64 ? -30 : 30);
    if (next >= MIN_OFFSET_MIN && next <= MAX_OFFSET_MIN) offsetMin_ = static_cast<int16_t>(next);
  } else if (row == 1) {
    size_t i = 0;
    while (i < 4 && TIMEOUTS_MS[i] != sleepMs_) ++i;
    sleepMs_ = TIMEOUTS_MS[(i + 1) % 4];
  } else {
    return true;
  }
  apply();
  save();
  return true;
}

void SettingsApp::render(Canvas& c, uint32_t) {
  drawHeader(c, "Settings");
  char buf[40];
  const int a = abs(offsetMin_);
  snprintf(buf, sizeof(buf), "< UTC %c%d:%02d >", offsetMin_ < 0 ? '-' : '+', a / 60, a % 60);
  c.drawText(4, headerH() + 2, buf);
  snprintf(buf, sizeof(buf), "Screen off: %us", (unsigned)(sleepMs_ / 1000));
  c.drawText(4, headerH() + rowH() + 2, buf);
  c.drawText(4, headerH() + 2 * rowH() + 2, "Time: serial `time`");
}
}  // namespace mos
