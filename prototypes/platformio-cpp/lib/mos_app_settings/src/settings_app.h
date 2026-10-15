#pragma once
// Settings: UTC offset and screen timeout, persisted through the OS store.
//   row 1: tap left half = -30 min, right half = +30 min
//   row 2: tap cycles 10 / 15 / 30 / 60 s
// Daylight saving is not automatic; adjust the offset by hand.
#include "mos_app.h"

namespace mos {
class SettingsApp : public App {
 public:
  const char* name() const override { return "Settings"; }
  const char* shortName() const override { return "SET"; }
  void onStart() override;
  bool onGesture(const GestureEvent& ev, uint32_t nowMs) override;
  void render(Canvas& canvas, uint32_t nowMs) override;

  static constexpr const char* KEY_UTC_OFFSET = "utc_off_min";
  static constexpr const char* KEY_SLEEP_MS = "sleep_ms";

 private:
  void apply();
  void save();
  int16_t offsetMin_ = 0;
  uint32_t sleepMs_ = 15000;
};
}  // namespace mos
