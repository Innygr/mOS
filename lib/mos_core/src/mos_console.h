#pragma once
// Serial console shared by bring-up and tests. Returns JSON Lines like Mission
// Link's console. It can inject fake messages and touches so the UI can be
// exercised with no radio or touch controller attached.
#include <string>
#include "mos_ui.h"

namespace mos {
struct ConsoleContext {
  MessagingService* svc;
  Ui* ui;
  WatchClock* clock;
  const DeviceInfo* dev;
  // Feeds a synthetic gesture through the same path real touch uses.
  typedef void (*InjectFn)(void* user, Gesture g, int16_t x, int16_t y);
  InjectFn injectGesture;
  void* user;
  ConsoleContext(MessagingService* s = nullptr, Ui* u = nullptr, WatchClock* c = nullptr,
                 const DeviceInfo* d = nullptr, InjectFn inj = nullptr, void* usr = nullptr)
      : svc(s), ui(u), clock(c), dev(d), injectGesture(inj), user(usr) {}
};

// Parses 16 hex chars into 8 bytes.
bool parseNodeId(const char* hex, uint8_t out[8]);
std::string handleCommand(const std::string& line, ConsoleContext& ctx, uint32_t nowMs);
}  // namespace mos
