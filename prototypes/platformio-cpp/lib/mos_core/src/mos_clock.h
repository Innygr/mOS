#pragma once
// Wall clock for the watch face. There is no RTC or network time yet (time sync
// is an open mOS question), so time is set manually or later by the mesh and
// then extrapolated from millis(). Daylight saving is not handled: set the
// offset yourself.
#include <stdint.h>

namespace mos {
class WatchClock {
 public:
  void setEpoch(uint32_t epochSeconds, uint32_t nowMs) {
    baseEpoch_ = epochSeconds;
    baseMs_ = nowMs;
    valid_ = true;
  }
  void setUtcOffsetMinutes(int16_t minutes) { offsetMin_ = minutes; }
  bool valid() const { return valid_; }
  // Local time of day; returns false if the clock has never been set.
  bool timeOfDay(uint32_t nowMs, uint8_t& h, uint8_t& m, uint8_t& s) const {
    if (!valid_) return false;
    const int64_t local = static_cast<int64_t>(baseEpoch_) + (nowMs - baseMs_) / 1000 +
                          static_cast<int64_t>(offsetMin_) * 60;
    int64_t sod = local % 86400;
    if (sod < 0) sod += 86400;
    h = static_cast<uint8_t>(sod / 3600);
    m = static_cast<uint8_t>((sod / 60) % 60);
    s = static_cast<uint8_t>(sod % 60);
    return true;
  }

 private:
  bool valid_ = false;
  uint32_t baseEpoch_ = 0;
  uint32_t baseMs_ = 0;
  int16_t offsetMin_ = 0;
};
}  // namespace mos
