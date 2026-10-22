#pragma once
// The mOS app interface. Apps (Mission Link, System, ...) plug into the Shell;
// they never touch hardware directly. They draw on a Canvas, receive gestures,
// and reach system services (clock, device info, the LoRa radio, notifications)
// through SystemContext.
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "mos_canvas.h"
#include "mos_clock.h"
#include "mos_gesture.h"
#include "mos_storage.h"
#include "missionlink_packet.h"

namespace mos {
enum class TxResult : uint8_t { Sent, Blocked, Error };

class IPacketSink {
 public:
  virtual ~IPacketSink() {}
  virtual TxResult transmit(const ml::Packet& packet) = 0;
};

class INotifier {
 public:
  virtual ~INotifier() {}
  virtual void notify(const char* text, uint32_t nowMs) = 0;
};

class IStore {
 public:
  virtual ~IStore() {}
  virtual bool getU32(const char* key, uint32_t& out) = 0;
  virtual bool putU32(const char* key, uint32_t value) = 0;
  virtual size_t getBlob(const char* key, uint8_t* buf, size_t cap) = 0;
  virtual bool putBlob(const char* key, const uint8_t* data, size_t len) = 0;
};

class IPowerControl {
 public:
  virtual ~IPowerControl() {}
  virtual void setSleepAfterMs(uint32_t ms) = 0;
  virtual uint32_t sleepAfterMs() const = 0;
};

struct DeviceInfo {
  const char* radioState = "unknown";
  bool rfEnabled = false;
  bool receiving = false;
  int16_t rssi = 0;
  int8_t snr = 0;
  const char* preset = "client";
  char nodeIdHex[17] = {0};
  int8_t batteryPercent = -1;
};

struct SystemContext {
  WatchClock* clock = nullptr;
  const DeviceInfo* device = nullptr;
  IPacketSink* radio = nullptr;
  INotifier* notifier = nullptr;
  IStore* store = nullptr;
  IPowerControl* power = nullptr;
  StorageService* storage = nullptr; // optional TF card; may be absent on any device
};

class App {
 public:
  virtual ~App() {}
  virtual const char* name() const = 0;
  virtual const char* shortName() const = 0;
  virtual int badge() const { return 0; }
  virtual void onStart() {}
  virtual void onOpen(uint32_t nowMs) { (void)nowMs; }
  virtual void tick(uint32_t nowMs) { (void)nowMs; }
  virtual bool onGesture(const GestureEvent& ev, uint32_t nowMs) {
    (void)ev; (void)nowMs; return false;
  }
  virtual void render(Canvas& canvas, uint32_t nowMs) = 0;
  virtual bool onPacket(const ml::Packet& p, int16_t rssi, uint32_t nowMs) {
    (void)p; (void)rssi; (void)nowMs; return false;
  }
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
