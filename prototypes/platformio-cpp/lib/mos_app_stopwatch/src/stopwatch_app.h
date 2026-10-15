#pragma once
// Stopwatch: tap to start/stop, long press to reset. Keeps running in the
// background and while the screen sleeps (it measures from millis()).
#include "mos_app.h"

namespace mos {
class StopwatchApp : public App {
 public:
  const char* name() const override { return "Stopwatch"; }
  const char* shortName() const override { return "SW"; }
  bool onGesture(const GestureEvent& ev, uint32_t nowMs) override;
  void render(Canvas& canvas, uint32_t nowMs) override;

  bool running() const { return running_; }
  uint32_t elapsedMs(uint32_t nowMs) const { return accumulated_ + (running_ ? nowMs - startedAt_ : 0); }

 private:
  bool running_ = false;
  uint32_t startedAt_ = 0;
  uint32_t accumulated_ = 0;
};
}  // namespace mos
