#include "mos_shell.h"
#include <stdio.h>
#include <string.h>

namespace mos {
Shell::Shell(Canvas& canvas, WatchClock& clock, const DeviceInfo& device, IPacketSink& radio)
    : cv_(canvas), clock_(clock), dev_(device) {
  radio_.target = &radio;
  ctx_.clock = &clock_;
  ctx_.device = &dev_;
  ctx_.radio = &radio_;
  ctx_.notifier = this;
}

bool Shell::addApp(App* a) {
  if (!a || count_ >= MAX_APPS) return false;
  apps_[count_++] = a;
  a->attach(ctx_);
  return true;
}

App* Shell::app(const char* name) const {
  for (size_t i = 0; i < count_; ++i)
    if (strcmp(apps_[i]->name(), name) == 0) return apps_[i];
  return nullptr;
}

bool Shell::open(const char* appName, uint32_t nowMs) {
  App* a = app(appName);
  if (!a) return false;
  running_ = a;
  view_ = View::App;
  a->onOpen(nowMs);
  return true;
}

void Shell::notify(const char* text, uint32_t nowMs) {
  lastActivityMs_ = nowMs;
  strncpy(toast_, text ? text : "", sizeof(toast_) - 1);
  toast_[sizeof(toast_) - 1] = '\0';
  toastUntilMs_ = nowMs + TOAST_MS;
}

void Shell::tick(uint32_t nowMs) {
  for (size_t i = 0; i < count_; ++i) apps_[i]->tick(nowMs);
}

bool Shell::dispatchPacket(const ml::Packet& p, int16_t rssi, uint32_t nowMs) {
  for (size_t i = 0; i < count_; ++i)
    if (apps_[i]->onPacket(p, rssi, nowMs)) return true;
  return false;
}

void Shell::onGesture(const GestureEvent& ev, uint32_t nowMs) {
  if (ev.gesture == Gesture::None) return;
  const bool wasAsleep = asleep(nowMs);
  lastActivityMs_ = nowMs;
  if (wasAsleep) { goHome(); return; }  // wake only
  switch (view_) {
    case View::Face:
      if (ev.gesture == Gesture::SwipeLeft) { view_ = View::Launcher; scroll_ = 0; }
      break;
    case View::Launcher: {
      const size_t rows = static_cast<size_t>(visibleRows(cv_));
      if (ev.gesture == Gesture::SwipeRight) goHome();
      else if (ev.gesture == Gesture::SwipeUp) { if (scroll_ + rows < count_) ++scroll_; }
      else if (ev.gesture == Gesture::SwipeDown) { if (scroll_ > 0) --scroll_; }
      else if (ev.gesture == Gesture::Tap && ev.y >= headerH()) {
        const size_t idx = scroll_ + static_cast<size_t>((ev.y - headerH()) / rowH());
        if (idx < count_) open(apps_[idx]->name(), nowMs);
      }
      break;
    }
    case View::App:
      if (running_ && running_->onGesture(ev, nowMs)) break;
      if (ev.gesture == Gesture::SwipeRight) { view_ = View::Launcher; running_ = nullptr; }
      else if (ev.gesture == Gesture::LongPress) goHome();
      break;
  }
}

void Shell::render(uint32_t nowMs) {
  cv_.clear();
  if (asleep(nowMs)) { cv_.present(); return; }  // firmware also powers the panel down
  switch (view_) {
    case View::Face: renderFace(nowMs); break;
    case View::Launcher: renderLauncher(); break;
    case View::App: if (running_) running_->render(cv_, nowMs); break;
  }
  if (nowMs < toastUntilMs_) {
    const int w = static_cast<int>(strlen(toast_)) * FONT_W + 4;
    const int x = (cv_.width() - w) / 2, y = cv_.height() - FONT_H - 4;
    cv_.fillRect(x, y, w, FONT_H + 3, false);
    cv_.drawRect(x, y, w, FONT_H + 3);
    cv_.drawText(x + 2, y + 2, toast_);
  }
  cv_.present();
}

void Shell::renderFace(uint32_t nowMs) {
  char buf[24];
  uint8_t h, m, s;
  if (clock_.timeOfDay(nowMs, h, m, s)) snprintf(buf, sizeof(buf), "%02u:%02u", h, m);
  else snprintf(buf, sizeof(buf), "--:--");
  const int scale = 3;
  const int tw = static_cast<int>(strlen(buf)) * FONT_W * scale;
  cv_.drawText((cv_.width() - tw) / 2, 8, buf, scale);
  if (!clock_.valid()) {
    const char* hint = "time not set";
    cv_.drawText((cv_.width() - static_cast<int>(strlen(hint)) * FONT_W) / 2, 36, hint);
  }
  const int y = cv_.height() - FONT_H - 1;
  int x = 2;
  for (size_t i = 0; i < count_; ++i) {  // app badges, e.g. "ML 2"
    if (apps_[i]->badge() <= 0) continue;
    snprintf(buf, sizeof(buf), "%s %d", apps_[i]->shortName(), apps_[i]->badge());
    cv_.drawText(x, y, buf);
    x += (static_cast<int>(strlen(buf)) + 1) * FONT_W;
  }
  const char* radio = dev_.receiving ? "RX" : "NO RADIO";
  cv_.drawText(cv_.width() - static_cast<int>(strlen(radio)) * FONT_W - 2, y, radio);
}

void Shell::renderLauncher() {
  drawHeader(cv_, "Apps");
  for (int r = 0; r < visibleRows(cv_); ++r) {
    const size_t i = scroll_ + static_cast<size_t>(r);
    if (i >= count_) break;
    char buf[40];
    if (apps_[i]->badge() > 0) snprintf(buf, sizeof(buf), "%s (%d)", apps_[i]->name(), apps_[i]->badge());
    else snprintf(buf, sizeof(buf), "%s", apps_[i]->name());
    cv_.drawText(4, headerH() + r * rowH() + 2, buf);
  }
}
}  // namespace mos
