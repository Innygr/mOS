#pragma once
// The Mission Link app for mOS: inbox, message reader, canned replies, and the
// messaging engine behind them. It talks to the LoRa radio only through the OS
// (SystemContext::radio) and learns about incoming packets via onPacket().
//
//   Inbox   : swipe up/down scroll, tap row -> Message   (swipe right exits the app)
//   Message : swipe up/down scroll, tap bottom bar -> Reply, swipe right -> Inbox
//   Reply   : swipe up/down scroll, tap phrase -> queue it to the sender, swipe right -> Message
#include "mos_app.h"
#include "mos_messaging.h"

namespace mos {
class MissionLinkApp : public App {
 public:
  MissionLinkApp(const uint8_t localId[8], uint32_t bootNonce,
                 const MessagingService::Config& cfg = MessagingService::Config());

  const char* name() const override { return "Mission Link"; }
  const char* shortName() const override { return "ML"; }
  int badge() const override { return static_cast<int>(svc_.inbox().unread()); }

  void onOpen(uint32_t nowMs) override;
  void tick(uint32_t nowMs) override;
  bool onGesture(const GestureEvent& ev, uint32_t nowMs) override;
  void render(Canvas& canvas, uint32_t nowMs) override;
  bool onPacket(const ml::Packet& p, int16_t rssi, uint32_t nowMs) override;
  bool onCommand(const std::string& cmd, const std::string& args, uint32_t nowMs,
                 std::string& reply) override;
  const char* commandUsage() const override {
    return "\"inbox\",\"outbox\",\"send <node-id-hex16> <text>\",\"broadcast <text>\","
           "\"sim <text>\"";
  }

  MessagingService& service() { return svc_; }

 private:
  enum class Page : uint8_t { Inbox, Message, Reply };
  MessagingService svc_;
  Page page_ = Page::Inbox;
  size_t scroll_ = 0;
  size_t selected_ = 0;
  Canvas* lastCanvas_ = nullptr;  // layout is derived from the canvas last drawn on

  void go(Page p) { page_ = p; scroll_ = 0; }
  int rows() const;
  int messageTextRows() const;
  std::vector<std::string> messageLines(const Canvas& c) const;
  void renderInbox(Canvas& c);
  void renderMessage(Canvas& c);
  void renderReply(Canvas& c);
};
}  // namespace mos
