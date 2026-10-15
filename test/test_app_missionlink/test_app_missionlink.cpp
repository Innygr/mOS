#include <unity.h>
#include <string.h>
#include <string>
#include <vector>
#include "rec_canvas.h"
#include "missionlink_app.h"
#include "mos_console.h"
#include "mos_shell.h"
#include "mesh_router.h"
#include "missionlink_config.h"

using namespace mos;

static const uint8_t ME[8] = {'M', 'L', 1, 2, 3, 4, 5, 6};
static const uint8_t PEER[8] = {'M', 'L', 9, 9, 0xab, 0xcd, 0x12, 0x34};

struct FakeSink : IPacketSink {
  TxResult next = TxResult::Sent;
  std::vector<ml::Packet> sent;
  TxResult transmit(const ml::Packet& p) override {
    if (next == TxResult::Sent) sent.push_back(p);
    return next;
  }
};

static ml::Packet dataFrom(const uint8_t src[8], const char* text, uint8_t idByte,
                           ml::AddressMode mode = ml::AddressMode::Direct) {
  ml::Packet p;
  p.mode = mode;
  memcpy(p.source, src, 8);
  memcpy(p.destination, ME, 8);
  p.messageId[7] = idByte;
  encodeText(text, p.payload);
  return p;
}

static MessagingService::Config armed() {
  MessagingService::Config c;
  c.encryptionReady = true;
  return c;
}

// ---- message payloads --------------------------------------------------------
void test_text_round_trip_and_sanitising() {
  std::vector<uint8_t> b;
  TEST_ASSERT_TRUE(encodeText("hello", b));
  char out[MAX_TEXT + 1];
  TEST_ASSERT_TRUE(decodeMessage(b, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("hello", out);
  b = {1, 'a', 0x07, 0xc3, 'z'};  // control + non-ASCII bytes
  TEST_ASSERT_TRUE(decodeMessage(b, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("a??z", out);
}
void test_text_limits() {
  std::vector<uint8_t> b;
  TEST_ASSERT_FALSE(encodeText("", b));
  TEST_ASSERT_FALSE(encodeText(nullptr, b));
  TEST_ASSERT_FALSE(encodeText(std::string(MAX_TEXT + 1, 'x').c_str(), b));
  TEST_ASSERT_TRUE(encodeText(std::string(MAX_TEXT, 'x').c_str(), b));
  TEST_ASSERT_TRUE(b.size() <= ML_MAX_PAYLOAD);
}
void test_canned_and_garbage() {
  std::vector<uint8_t> b;
  char out[MAX_TEXT + 1];
  TEST_ASSERT_TRUE(encodeCanned(4, b));
  TEST_ASSERT_EQUAL_UINT32(2, b.size());
  TEST_ASSERT_TRUE(decodeMessage(b, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("On my way", out);
  TEST_ASSERT_FALSE(encodeCanned(0, b));
  TEST_ASSERT_FALSE(encodeCanned(CANNED_COUNT + 1, b));
  TEST_ASSERT_FALSE(decodeMessage({}, out, sizeof(out)));
  TEST_ASSERT_FALSE(decodeMessage({9, 1}, out, sizeof(out)));
  TEST_ASSERT_FALSE(decodeMessage({2, 99}, out, sizeof(out)));
  TEST_ASSERT_FALSE(decodeMessage({2, 1, 1}, out, sizeof(out)));
  TEST_ASSERT_FALSE(decodeMessage({1}, out, sizeof(out)));
}

// ---- inbox -------------------------------------------------------------------
void test_inbox_ring_newest_first_and_wraps() {
  Inbox in;
  for (int i = 0; i < 20; ++i) {
    char t[8];
    snprintf(t, sizeof(t), "m%d", i);
    in.push(PEER, ml::AddressMode::Direct, t, i, -80);
  }
  TEST_ASSERT_EQUAL_UINT32(Inbox::CAPACITY, in.count());
  TEST_ASSERT_EQUAL_STRING("m19", in.at(0)->text);
  TEST_ASSERT_EQUAL_STRING("m4", in.at(Inbox::CAPACITY - 1)->text);
  TEST_ASSERT_NULL(in.at(Inbox::CAPACITY));
  TEST_ASSERT_EQUAL_UINT32(Inbox::CAPACITY, in.unread());
  in.markRead(0);
  TEST_ASSERT_EQUAL_UINT32(Inbox::CAPACITY - 1, in.unread());
}

// ---- messaging ----------------------------------------------------------------
void test_receive_direct_stores_and_queues_ack() {
  MessagingService svc(ME, 0x11223344);
  auto p = dataFrom(PEER, "hi", 5);
  TEST_ASSERT_EQUAL((int)MessagingService::RxOutcome::Stored, (int)svc.onReceive(p, -91, 100));
  TEST_ASSERT_EQUAL_UINT32(1, svc.inbox().count());
  TEST_ASSERT_EQUAL_STRING("hi", svc.inbox().at(0)->text);
  TEST_ASSERT_EQUAL_UINT32(1, svc.pending());
  const OutItem& ack = svc.outbox(0);
  TEST_ASSERT_EQUAL((int)ml::PacketType::Ack, (int)ack.packet.type);
  TEST_ASSERT_EQUAL_MEMORY(PEER, ack.packet.destination, 8);
  TEST_ASSERT_EQUAL_MEMORY(p.messageId, ack.packet.payload.data(), 8);
}
void test_broadcast_receive_queues_no_ack() {
  MessagingService svc(ME, 1);
  svc.onReceive(dataFrom(PEER, "all", 5, ml::AddressMode::Broadcast), -90, 0);
  TEST_ASSERT_EQUAL_UINT32(1, svc.inbox().count());
  TEST_ASSERT_EQUAL_UINT32(0, svc.pending());
}
void test_undecodable_data_ignored() {
  MessagingService svc(ME, 1);
  auto p = dataFrom(PEER, "x", 5);
  p.payload = {7, 7, 7};
  TEST_ASSERT_EQUAL((int)MessagingService::RxOutcome::Ignored, (int)svc.onReceive(p, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(0, svc.inbox().count());
}
void test_message_ids_nonzero_and_unique() {
  MessagingService svc(ME, 0);  // even a zero nonce must give a valid id
  TEST_ASSERT_TRUE(svc.sendText(PEER, "a", 0));
  TEST_ASSERT_TRUE(svc.sendText(PEER, "b", 0));
  static const uint8_t zero[8] = {0};
  TEST_ASSERT_TRUE(memcmp(svc.outbox(0).packet.messageId, zero, 8) != 0);
  TEST_ASSERT_TRUE(memcmp(svc.outbox(0).packet.messageId, svc.outbox(1).packet.messageId, 8) != 0);
}
void test_never_transmits_plaintext_by_default() {
  MessagingService svc(ME, 1);  // encryptionReady == false
  FakeSink sink;
  svc.sendText(PEER, "secret", 0);
  svc.tick(1000, sink);
  TEST_ASSERT_EQUAL_UINT32(0, sink.sent.size());
  TEST_ASSERT_TRUE(svc.txBlocked());
  TEST_ASSERT_EQUAL((int)OutState::Queued, (int)svc.outbox(0).state);
  TEST_ASSERT_EQUAL_UINT8(0, svc.outbox(0).attempts);
}
void test_blocked_radio_does_not_burn_attempts() {
  MessagingService svc(ME, 1, armed());
  FakeSink sink;
  sink.next = TxResult::Blocked;
  svc.sendText(PEER, "hi", 0);
  for (uint32_t t = 0; t < 10; ++t) svc.tick(t * 1000, sink);
  TEST_ASSERT_EQUAL((int)OutState::Queued, (int)svc.outbox(0).state);
  TEST_ASSERT_EQUAL_UINT8(0, svc.outbox(0).attempts);
  TEST_ASSERT_TRUE(svc.txBlocked());
  sink.next = TxResult::Sent;
  svc.tick(20000, sink);
  TEST_ASSERT_EQUAL((int)OutState::AwaitingAck, (int)svc.outbox(0).state);
  TEST_ASSERT_FALSE(svc.txBlocked());
}
void test_ack_completes_and_checks_sender() {
  MessagingService svc(ME, 1, armed());
  FakeSink sink;
  svc.sendText(PEER, "hi", 0);
  svc.tick(0, sink);
  TEST_ASSERT_EQUAL_UINT32(1, sink.sent.size());
  ml::Packet ack;
  ack.type = ml::PacketType::Ack;
  memcpy(ack.destination, ME, 8);
  ack.payload.assign(sink.sent[0].messageId, sink.sent[0].messageId + 8);
  static const uint8_t other[8] = {'M', 'L', 7, 7, 7, 7, 7, 7};
  memcpy(ack.source, other, 8);  // wrong node must not be able to ack
  ack.messageId[7] = 1;
  TEST_ASSERT_EQUAL((int)MessagingService::RxOutcome::Ignored, (int)svc.onReceive(ack, 0, 10));
  memcpy(ack.source, PEER, 8);
  TEST_ASSERT_EQUAL((int)MessagingService::RxOutcome::AckHandled, (int)svc.onReceive(ack, 0, 10));
  TEST_ASSERT_EQUAL((int)OutState::Acked, (int)svc.outbox(0).state);
  TEST_ASSERT_EQUAL_UINT32(0, svc.pending());
}
void test_retry_backoff_then_fail() {
  MessagingService::Config c = armed();
  MessagingService svc(ME, 1, c);
  FakeSink sink;
  svc.sendText(PEER, "hi", 0);
  svc.tick(0, sink);                          // attempt 1
  svc.tick(c.ackTimeoutMs - 1, sink);         // not yet
  TEST_ASSERT_EQUAL_UINT32(1, sink.sent.size());
  svc.tick(c.ackTimeoutMs, sink);             // attempt 2
  TEST_ASSERT_EQUAL_UINT32(2, sink.sent.size());
  svc.tick(c.ackTimeoutMs + c.ackTimeoutMs * 2, sink);  // attempt 3 (doubled wait)
  TEST_ASSERT_EQUAL_UINT32(3, sink.sent.size());
  svc.tick(1000000, sink);                    // out of attempts
  TEST_ASSERT_EQUAL_UINT32(3, sink.sent.size());
  TEST_ASSERT_EQUAL((int)OutState::Failed, (int)svc.outbox(0).state);
}
void test_tx_error_retries_then_fails() {
  MessagingService::Config c = armed();
  MessagingService svc(ME, 1, c);
  FakeSink sink;
  sink.next = TxResult::Error;
  svc.sendText(PEER, "hi", 0);
  svc.tick(0, sink);
  TEST_ASSERT_EQUAL_UINT8(1, svc.outbox(0).attempts);
  svc.tick(c.errorBackoffMs, sink);
  svc.tick(2 * c.errorBackoffMs, sink);
  TEST_ASSERT_EQUAL((int)OutState::Failed, (int)svc.outbox(0).state);
}
void test_outbox_full_then_reclaims_finished() {
  MessagingService svc(ME, 1, armed());
  FakeSink sink;
  for (size_t i = 0; i < MessagingService::OUTBOX_CAPACITY; ++i) TEST_ASSERT_TRUE(svc.broadcastText("x", 0));
  TEST_ASSERT_FALSE(svc.broadcastText("overflow", 0));
  svc.tick(0, sink);  // broadcasts need no ack -> Sent (finished)
  TEST_ASSERT_TRUE(svc.broadcastText("fits now", 1));
}


// ---- the app running inside the OS shell ----------------------------------------
struct OsFixture {
  RecCanvas cv;
  WatchClock clock;
  DeviceInfo dev;
  FakeSink radio;
  Shell shell{cv, clock, dev, radio};
  MissionLinkApp ml;
  OsFixture() : ml(ME, 7) { shell.addApp(&ml); }
  void gesture(Gesture g, uint32_t t, int x = 0, int y = 0) {
    shell.render(t);  // the app lays out against the canvas it last drew on
    shell.onGesture(GestureEvent(g, (int16_t)x, (int16_t)y), t);
  }
  // Launcher row 0 -> Mission Link.
  void openMl(uint32_t t) {
    gesture(Gesture::SwipeLeft, t);
    gesture(Gesture::Tap, t, 30, 14);
  }
};

void test_app_open_read_and_reply_flow() {
  OsFixture f;
  f.shell.dispatchPacket(dataFrom(PEER, "Meet at the north ridge node at noon please", 1, ml::AddressMode::Broadcast), -90, 50);
  f.shell.render(60);
  TEST_ASSERT_TRUE(f.cv.has("ML 1"));        // face badge
  TEST_ASSERT_TRUE(f.cv.has("New message")); // toast from the app via the OS notifier
  f.openMl(100);
  TEST_ASSERT_EQUAL((int)View::App, (int)f.shell.view());
  f.shell.render(110);
  TEST_ASSERT_TRUE(f.cv.has("Inbox (1)"));
  f.gesture(Gesture::Tap, 200, 30, 14);  // first row
  TEST_ASSERT_EQUAL_UINT32(0, f.ml.service().inbox().unread());
  f.shell.render(210);
  TEST_ASSERT_TRUE(f.cv.has("From abcd1234"));
  TEST_ASSERT_TRUE(f.cv.has("Meet at the north"));
  f.gesture(Gesture::Tap, 300, 60, 58);  // reply bar
  f.shell.render(310);
  TEST_ASSERT_TRUE(f.cv.has("Reply with"));
  f.gesture(Gesture::Tap, 400, 30, 12 + 12 * 3 + 2);  // 4th phrase, "On my way"
  TEST_ASSERT_EQUAL_UINT32(1, f.ml.service().pending());
  const OutItem& o = f.ml.service().outbox(0);
  TEST_ASSERT_EQUAL((int)ml::AddressMode::Direct, (int)o.packet.mode);
  TEST_ASSERT_EQUAL_MEMORY(PEER, o.packet.destination, 8);
  char text[MAX_TEXT + 1];
  decodeMessage(o.packet.payload, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("On my way", text);
  f.shell.render(410);
  TEST_ASSERT_TRUE(f.cv.has("Queued"));
  TEST_ASSERT_TRUE(f.cv.has("From abcd1234"));  // back on the message page
}
void test_app_back_navigation_then_os_takes_over() {
  OsFixture f;
  f.shell.dispatchPacket(dataFrom(PEER, "hi", 1, ml::AddressMode::Broadcast), -90, 0);
  f.openMl(10);
  f.gesture(Gesture::Tap, 20, 30, 14);              // -> Message
  f.gesture(Gesture::SwipeRight, 30);               // app consumes: -> Inbox
  TEST_ASSERT_EQUAL((int)View::App, (int)f.shell.view());
  f.shell.render(35);
  TEST_ASSERT_TRUE(f.cv.has("Inbox (1)"));
  f.gesture(Gesture::SwipeRight, 40);               // app declines: OS -> Launcher
  TEST_ASSERT_EQUAL((int)View::Launcher, (int)f.shell.view());
}
void test_app_reply_scroll_reaches_last_phrase() {
  OsFixture f;
  f.shell.dispatchPacket(dataFrom(PEER, "hi", 1, ml::AddressMode::Broadcast), -90, 0);
  f.openMl(10);
  f.gesture(Gesture::Tap, 20, 30, 14);
  f.gesture(Gesture::Tap, 30, 60, 58);
  for (int i = 0; i < 3; ++i) f.gesture(Gesture::SwipeUp, 40 + i);  // clamps
  f.shell.render(70);
  TEST_ASSERT_TRUE(f.cv.has("Call me"));
  f.gesture(Gesture::Tap, 80, 30, 12 + 12 * 3 + 2);
  char text[MAX_TEXT + 1];
  decodeMessage(f.ml.service().outbox(0).packet.payload, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Call me", text);
}
void test_background_tick_runs_while_not_foreground_and_never_sends_plaintext() {
  OsFixture f;                       // default config: encryption not ready
  f.shell.dispatchPacket(dataFrom(PEER, "hi", 1), -90, 0);  // queues an ack
  f.shell.tick(5000);
  TEST_ASSERT_EQUAL_UINT32(0, f.radio.sent.size());
  TEST_ASSERT_TRUE(f.ml.service().txBlocked());
  MissionLinkApp armedApp(ME, 7, armed());
  OsFixture g;
  Shell s2(g.cv, g.clock, g.dev, g.radio);
  s2.addApp(&armedApp);
  s2.dispatchPacket(dataFrom(PEER, "hi", 1), -90, 0);
  s2.tick(5000);                     // the app is in the background, yet it still transmits
  TEST_ASSERT_EQUAL_UINT32(1, g.radio.sent.size());
  TEST_ASSERT_EQUAL((int)ml::PacketType::Ack, (int)g.radio.sent[0].type);
}

// ---- console commands contributed by the app ---------------------------------
struct ConFixture : OsFixture {
  std::string run(const char* line, uint32_t t = 1000) { return handleCommand(line, shell, clock, dev, t); }
};
void test_console_app_commands() {
  ConFixture f;
  TEST_ASSERT_TRUE(f.run("help").find("send <node-id-hex16>") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("send zz hi").find("bad_args") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("send 4d4c090900000001 hello").find("queued") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT32(1, f.ml.service().pending());
  TEST_ASSERT_TRUE(f.run("broadcast ").find("rejected") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("outbox").find("\"pending\":1") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("sim say \"hi\" \\ there").find("stored") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT32(1, f.ml.service().inbox().unread());
  TEST_ASSERT_TRUE(f.run("inbox").find("say \\\"hi\\\" \\\\ there") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("apps").find("\"badge\":1") != std::string::npos);
}

// ---- protocol interop: watch (client) against Mission Link router -----------
void test_watch_router_delivers_direct_and_drops_transit() {
  ml::MeshRouter router(ml::NodePreset::Client);
  ml::Packet fwd;
  auto mine = dataFrom(PEER, "hi", 1);
  TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverLocal, (int)router.process(mine, ME, 10, fwd));
  auto transit = dataFrom(PEER, "hi", 2);
  static const uint8_t other[8] = {'M', 'L', 5, 5, 5, 5, 5, 5};
  memcpy(transit.destination, other, 8);
  TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid, (int)router.process(transit, ME, 11, fwd));
  // What the watch queues must be a valid frame the codec and router accept.
  MessagingService svc(ME, 3);
  svc.sendCanned(PEER, 1, 0);
  std::vector<uint8_t> wire;
  TEST_ASSERT_TRUE(ml::encode(svc.outbox(0).packet, wire));
  ml::Packet back;
  TEST_ASSERT_TRUE(ml::decode(wire.data(), wire.size(), back));
  ml::MeshRouter peerRouter(ml::NodePreset::Client);
  TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverLocal, (int)peerRouter.process(back, PEER, 20, fwd));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_text_round_trip_and_sanitising);
  RUN_TEST(test_text_limits);
  RUN_TEST(test_canned_and_garbage);
  RUN_TEST(test_inbox_ring_newest_first_and_wraps);
  RUN_TEST(test_receive_direct_stores_and_queues_ack);
  RUN_TEST(test_broadcast_receive_queues_no_ack);
  RUN_TEST(test_undecodable_data_ignored);
  RUN_TEST(test_message_ids_nonzero_and_unique);
  RUN_TEST(test_never_transmits_plaintext_by_default);
  RUN_TEST(test_blocked_radio_does_not_burn_attempts);
  RUN_TEST(test_ack_completes_and_checks_sender);
  RUN_TEST(test_retry_backoff_then_fail);
  RUN_TEST(test_tx_error_retries_then_fails);
  RUN_TEST(test_outbox_full_then_reclaims_finished);
  RUN_TEST(test_app_open_read_and_reply_flow);
  RUN_TEST(test_app_back_navigation_then_os_takes_over);
  RUN_TEST(test_app_reply_scroll_reaches_last_phrase);
  RUN_TEST(test_background_tick_runs_while_not_foreground_and_never_sends_plaintext);
  RUN_TEST(test_console_app_commands);
  RUN_TEST(test_watch_router_delivers_direct_and_drops_transit);
  return UNITY_END();
}
