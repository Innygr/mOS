#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "mos_canvas.h"
#include "mos_clock.h"
#include "mos_gesture.h"
#include "mos_messaging.h"

namespace mos {
// Snapshot of radio/device state, filled in by firmware each loop.
struct DeviceInfo {
  const char* radioState = "unknown";
  bool rfEnabled = false;
  bool receiving = false;
  int16_t rssi = 0;
  int8_t snr = 0;
  const char* preset = "client";
  char nodeIdHex[17] = {0};
  int8_t batteryPercent = -1;  // -1 = unknown
};

enum class Screen : uint8_t { Face, Inbox, Message, Reply, Status };

// Greedy word wrap into lines of at most `cols` characters (long words split).
std::vector<std::string> wrapText(const char* text, int cols);

// Gesture map:
//   Face    : swipe left -> Inbox, swipe up -> Status
//   Inbox   : swipe up/down scroll, tap row -> Message, swipe right -> Face
//   Message : swipe up/down scroll, tap bottom bar -> Reply, swipe right -> Inbox
//   Reply   : tap phrase -> queue it to the sender, swipe right -> Message
//   Status  : swipe right/down -> Face
// The first touch after the display sleeps only wakes it and is not acted on.
class Ui {
 public:
  static constexpr uint32_t SLEEP_AFTER_MS = 15000;
  static constexpr uint32_t TOAST_MS = 1500;

  Ui(Canvas& canvas, MessagingService& svc, const WatchClock& clock);

  void setDevice(const DeviceInfo& info) { dev_ = info; }
  void onGesture(const GestureEvent& ev, uint32_t nowMs);
  void notifyIncoming(uint32_t nowMs);  // wakes the screen, shows a toast
  void render(uint32_t nowMs);

  bool asleep(uint32_t nowMs) const { return nowMs - lastActivityMs_ >= SLEEP_AFTER_MS; }
  Screen screen() const { return screen_; }

 private:
  Canvas& cv_;
  MessagingService& svc_;
  const WatchClock& clock_;
  DeviceInfo dev_;
  Screen screen_ = Screen::Face;
  size_t scroll_ = 0;       // first visible row / text line
  size_t selected_ = 0;     // inbox index of the open message
  uint32_t lastActivityMs_ = 0;
  uint32_t toastUntilMs_ = 0;
  char toast_[22] = {0};

  int headerH() const { return FONT_H + 2; }
  int rowH() const { return FONT_H + 4; }
  int visibleRows() const { return (cv_.height() - headerH()) / rowH(); }
  int cols() const { return cv_.width() / FONT_W; }
  void showToast(const char* text, uint32_t nowMs);
  void go(Screen s) { screen_ = s; scroll_ = 0; }
  void header(const char* title);
  void renderFace(uint32_t nowMs);
  void renderInbox();
  void renderMessage();
  void renderReply();
  void renderStatus();
  void handleFace(const GestureEvent& ev);
  void handleInbox(const GestureEvent& ev);
  void handleMessage(const GestureEvent& ev, uint32_t nowMs);
  void handleReply(const GestureEvent& ev, uint32_t nowMs);
  void handleStatus(const GestureEvent& ev);
  std::vector<std::string> messageLines() const;
  int messageTextRows() const;
};
}  // namespace mos
