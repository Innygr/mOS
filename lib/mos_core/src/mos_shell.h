#pragma once
#include <stddef.h>
#include <stdint.h>
#include "mos_app.h"
#include "mos_layout.h"

namespace mos {
enum class View : uint8_t { Face, Launcher, App };

// The OS shell: watch face, launcher, app lifecycle, toasts, screen sleep, and
// routing of radio packets / background ticks to apps.
//
//   Face     : swipe left -> Launcher
//   Launcher : swipe up/down scroll, tap row -> open app, swipe right -> Face
//   App      : unhandled swipe right -> Launcher, unhandled long press -> Face
// The first touch after the display sleeps only wakes it and is not acted on.
class Shell : public INotifier {
 public:
  static constexpr size_t MAX_APPS = 8;
  static constexpr uint32_t SLEEP_AFTER_MS = 15000;
  static constexpr uint32_t TOAST_MS = 1500;

  Shell(Canvas& canvas, WatchClock& clock, const DeviceInfo& device, IPacketSink& radio);

  bool addApp(App* app);  // false when full
  size_t appCount() const { return count_; }
  App* appAt(size_t i) const { return i < count_ ? apps_[i] : nullptr; }
  App* app(const char* name) const;

  void onGesture(const GestureEvent& ev, uint32_t nowMs);
  void tick(uint32_t nowMs);  // background work for every app
  // Hands a locally-delivered packet to apps; true if one consumed it.
  bool dispatchPacket(const ml::Packet& p, int16_t rssi, uint32_t nowMs);
  void render(uint32_t nowMs);
  void notify(const char* text, uint32_t nowMs) override;
  bool open(const char* appName, uint32_t nowMs);
  void goHome() { view_ = View::Face; running_ = nullptr; }

  bool asleep(uint32_t nowMs) const { return nowMs - lastActivityMs_ >= SLEEP_AFTER_MS; }
  View view() const { return view_; }
  App* running() const { return running_; }

 private:
  Canvas& cv_;
  WatchClock& clock_;
  const DeviceInfo& dev_;
  SystemContext ctx_;
  App* apps_[MAX_APPS] = {};
  size_t count_ = 0;
  View view_ = View::Face;
  App* running_ = nullptr;
  size_t scroll_ = 0;
  uint32_t lastActivityMs_ = 0;
  uint32_t toastUntilMs_ = 0;
  char toast_[22] = {0};
  struct RadioProxy : IPacketSink {
    IPacketSink* target = nullptr;
    TxResult transmit(const ml::Packet& p) override { return target->transmit(p); }
  } radio_;

  void renderFace(uint32_t nowMs);
  void renderLauncher();
};
}  // namespace mos
