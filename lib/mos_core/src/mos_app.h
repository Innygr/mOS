#pragma once
// The mOS app interface. Apps (Mission Link, System, ...) plug into the Shell;
// they never touch hardware directly. They draw on a Canvas, receive gestures,
// and reach system services (clock, device info, the LoRa radio, notifications)
// through SystemContext.
#include <stdint.h>
#include <string>
#include "mos_canvas.h"
#include "mos_clock.h"
#include "mos_gesture.h"
#include "missionlink_packet.h"

namespace mos {
// ---- system services ---------------------------------------------------------
enum class TxResult : uint8_t { Sent, Blocked, Error };

// The radio belongs to the OS. Apps hand it packets; the OS decides whether and
// when they go on air (currently never: ML_RF_ENABLED=0).
class IPacketSink {
 public:
  virtual ~IPacketSink() {}
  virtual TxResult transmit(const ml::Packet& packet) = 0;
};

class INotifier {
 public:
  virtual ~INotifier() {}
  // Wakes the screen and shows a short toast (truncated to fit).
  virtual void notify(const char* text, uint32_t nowMs) = 0;
};

// Snapshot of device/radio state, filled in by firmware each loop.
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

struct SystemContext {
  WatchClock* clock = nullptr;
  const DeviceInfo* device = nullptr;
  IPacketSink* radio = nullptr;
  INotifier* notifier = nullptr;
};

// ---- apps --------------------------------------------------------------------
class App {
 public:
  virtual ~App() {}
  virtual const char* name() const = 0;        // launcher label
  virtual const char* shortName() const = 0;   // watch-face badge label, <= 4 chars
  virtual int badge() const { return 0; }      // e.g. unread count; >0 shows on the face

  virtual void onOpen(uint32_t nowMs) { (void)nowMs; }          // launched from the launcher
  virtual void tick(uint32_t nowMs) { (void)nowMs; }            // every loop, even in background
  // Return true if the gesture was handled. An unhandled SwipeRight closes the app
  // and an unhandled LongPress goes home, so apps only consume what they use.
  virtual bool onGesture(const GestureEvent& ev, uint32_t nowMs) {
    (void)ev; (void)nowMs; return false;
  }
  virtual void render(Canvas& canvas, uint32_t nowMs) = 0;

  // Packets the router delivered to this node. Return true if the app consumed it.
  virtual bool onPacket(const ml::Packet& p, int16_t rssi, uint32_t nowMs) {
    (void)p; (void)rssi; (void)nowMs; return false;
  }

  // Serial console extension. Return true if `cmd` was handled; put a JSON line
  // in `reply`. `commandUsage()` is a comma-separated list of quoted usage strings.
  virtual bool onCommand(const std::string& cmd, const std::string& args, uint32_t nowMs,
                         std::string& reply) {
    (void)cmd; (void)args; (void)nowMs; (void)reply; return false;
  }
  virtual const char* commandUsage() const { return ""; }

  void attach(const SystemContext& ctx) { sys_ = ctx; }

 protected:
  const SystemContext& sys() const { return sys_; }

 private:
  SystemContext sys_;
};
}  // namespace mos
