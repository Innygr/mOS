#include "mos_gesture.h"

namespace mos {
static int16_t absv(int16_t v) { return v < 0 ? -v : v; }

void GestureRecognizer::touchDown(int16_t x, int16_t y, uint32_t nowMs) {
  active_ = true;
  sx_ = lx_ = x;
  sy_ = ly_ = y;
  downMs_ = nowMs;
}

void GestureRecognizer::touchMove(int16_t x, int16_t y) {
  if (!active_) return;
  lx_ = x;
  ly_ = y;
}

GestureEvent GestureRecognizer::touchUp(uint32_t nowMs) {
  GestureEvent ev;
  if (!active_) return ev;
  active_ = false;
  ev.x = sx_;
  ev.y = sy_;
  const int16_t dx = lx_ - sx_, dy = ly_ - sy_;
  const int16_t ax = absv(dx), ay = absv(dy);
  if (ax <= TAP_MAX_PX && ay <= TAP_MAX_PX) {
    ev.gesture = (nowMs - downMs_ >= LONG_PRESS_MS) ? Gesture::LongPress : Gesture::Tap;
  } else if (ax >= ay && ax >= SWIPE_MIN_PX) {
    ev.gesture = dx < 0 ? Gesture::SwipeLeft : Gesture::SwipeRight;
  } else if (ay > ax && ay >= SWIPE_MIN_PX) {
    ev.gesture = dy < 0 ? Gesture::SwipeUp : Gesture::SwipeDown;
  }  // else: drifted too far for a tap but not far enough for a swipe -> None
  return ev;
}
}  // namespace mos
