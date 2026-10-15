#pragma once
// Turns raw touch samples (down / move / up) into watch gestures.
#include <stdint.h>

namespace mos {
enum class Gesture : uint8_t { None, Tap, LongPress, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };

struct GestureEvent {
  Gesture gesture;
  int16_t x, y;  // start point of the touch
  GestureEvent(Gesture g = Gesture::None, int16_t px = 0, int16_t py = 0)
      : gesture(g), x(px), y(py) {}
};

// Swipe names describe finger motion: SwipeLeft = finger moved toward x=0.
class GestureRecognizer {
 public:
  static constexpr int16_t TAP_MAX_PX = 6;
  static constexpr int16_t SWIPE_MIN_PX = 16;
  static constexpr uint32_t LONG_PRESS_MS = 600;

  void touchDown(int16_t x, int16_t y, uint32_t nowMs);
  void touchMove(int16_t x, int16_t y);
  GestureEvent touchUp(uint32_t nowMs);
  bool active() const { return active_; }

 private:
  bool active_ = false;
  int16_t sx_ = 0, sy_ = 0, lx_ = 0, ly_ = 0;
  uint32_t downMs_ = 0;
};
}  // namespace mos
