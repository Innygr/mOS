#include <unity.h>
#include <string.h>
#include <string>
#include <vector>
#include "mos_console.h"
#include "mos_messaging.h"
#include "mos_ui.h"
#include "missionlink_config.h"

using namespace mos;

static const uint8_t ME[8] = {'M', 'L', 1, 2, 3, 4, 5, 6};
static const uint8_t PEER[8] = {'M', 'L', 9, 9, 0xab, 0xcd, 0x12, 0x34};

// ---- test doubles ----------------------------------------------------------
struct FakeSink : IPacketSink {
  TxResult next = TxResult::Sent;
  std::vector<ml::Packet> sent;
  TxResult transmit(const ml::Packet& p) override {
    if (next == TxResult::Sent) sent.push_back(p);
    return next;
  }
};

struct RecCanvas : Canvas {
  std::vector<std::string> texts;
  int fills = 0, presents = 0;
  int width() const override { return 128; }
  int height() const override { return 64; }
  void clear() override { texts.clear(); fills = 0; }
  void drawText(int, int, const char* t, int, bool) override { texts.push_back(t); }
  void fillRect(int, int, int, int, bool) override { ++fills; }
  void drawRect(int, int, int, int) override {}
  void present() override { ++presents; }
  bool has(const char* needle) const {
    for (auto& t : texts) if (t.find(needle) != std::string::npos) return true;
    return false;
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

// ---- gestures ----------------------------------------------------------------
static GestureEvent gesture(int sx, int sy, int ex, int ey, uint32_t ms) {
  GestureRecognizer g;
  g.touchDown(sx, sy, 0);
  g.touchMove((sx + ex) / 2, (sy + ey) / 2);
  g.touchMove(ex, ey);
  return g.touchUp(ms);
}
void test_gesture_classification() {
  TEST_ASSERT_EQUAL((int)Gesture::Tap, (int)gesture(50, 30, 52, 31, 100).gesture);
  TEST_ASSERT_EQUAL((int)Gesture::LongPress, (int)gesture(50, 30, 50, 30, 700).gesture);
  TEST_ASSERT_EQUAL((int)Gesture::SwipeLeft, (int)gesture(100, 30, 40, 32, 200).gesture);
  TEST_ASSERT_EQUAL((int)Gesture::SwipeRight, (int)gesture(20, 30, 90, 28, 200).gesture);
  TEST_ASSERT_EQUAL((int)Gesture::SwipeUp, (int)gesture(60, 55, 62, 10, 200).gesture);
  TEST_ASSERT_EQUAL((int)Gesture::SwipeDown, (int)gesture(60, 10, 58, 55, 200).gesture);
  TEST_ASSERT_EQUAL((int)Gesture::None, (int)gesture(60, 30, 60, 42, 100).gesture);  // 12px: neither
  GestureRecognizer g;
  TEST_ASSERT_EQUAL((int)Gesture::None, (int)g.touchUp(0).gesture);  // up without down
}

// ---- clock -------------------------------------------------------------------
void test_clock() {
  WatchClock c;
  uint8_t h, m, s;
  TEST_ASSERT_FALSE(c.timeOfDay(0, h, m, s));
  c.setEpoch(86400 * 3 + 3600 * 23 + 59 * 60 + 50, 1000);  // 23:59:50 UTC
  TEST_ASSERT_TRUE(c.timeOfDay(1000, h, m, s));
  TEST_ASSERT_EQUAL_UINT8(23, h); TEST_ASSERT_EQUAL_UINT8(59, m); TEST_ASSERT_EQUAL_UINT8(50, s);
  c.timeOfDay(11000, h, m, s);  // +10 s wraps past midnight
  TEST_ASSERT_EQUAL_UINT8(0, h); TEST_ASSERT_EQUAL_UINT8(0, m);
  c.setUtcOffsetMinutes(-480);
  c.timeOfDay(1000, h, m, s);
  TEST_ASSERT_EQUAL_UINT8(15, h); TEST_ASSERT_EQUAL_UINT8(59, m);
}

// ---- UI ------------------------------------------------------------------------
struct UiFixture {
  RecCanvas cv;
  MessagingService svc{ME, 7};
  WatchClock clock;
  Ui ui{cv, svc, clock};
  void tap(int x, int y, uint32_t t) { ui.onGesture({Gesture::Tap, (int16_t)x, (int16_t)y}, t); }
  void swipe(Gesture g, uint32_t t) { ui.onGesture({g, 0, 0}, t); }
};

void test_wrap_text() {
  auto l = wrapText("the quick brown fox", 9);
  TEST_ASSERT_EQUAL_UINT32(2, l.size());
  TEST_ASSERT_EQUAL_STRING("the quick", l[0].c_str());
  TEST_ASSERT_EQUAL_STRING("brown fox", l[1].c_str());
  l = wrapText("abcdefghijkl", 5);
  TEST_ASSERT_EQUAL_UINT32(3, l.size());
  TEST_ASSERT_EQUAL_STRING("fghij", l[1].c_str());
  TEST_ASSERT_EQUAL_UINT32(0, wrapText("   ", 5).size());
}
void test_ui_navigation_and_wake() {
  UiFixture f;
  f.ui.render(100);
  TEST_ASSERT_TRUE(f.cv.has("--:--"));
  f.swipe(Gesture::SwipeLeft, 200);
  TEST_ASSERT_EQUAL((int)Screen::Inbox, (int)f.ui.screen());
  f.swipe(Gesture::SwipeRight, 300);
  f.swipe(Gesture::SwipeUp, 400);
  TEST_ASSERT_EQUAL((int)Screen::Status, (int)f.ui.screen());
  f.ui.render(500);
  TEST_ASSERT_TRUE(f.cv.has("TX OFF"));
  f.swipe(Gesture::SwipeDown, 600);
  TEST_ASSERT_EQUAL((int)Screen::Face, (int)f.ui.screen());
  // After sleeping, the first touch only wakes and does not navigate.
  uint32_t later = 600 + Ui::SLEEP_AFTER_MS + 1;
  TEST_ASSERT_TRUE(f.ui.asleep(later));
  f.swipe(Gesture::SwipeLeft, later);
  TEST_ASSERT_EQUAL((int)Screen::Face, (int)f.ui.screen());
  TEST_ASSERT_FALSE(f.ui.asleep(later));
  f.swipe(Gesture::SwipeLeft, later + 10);
  TEST_ASSERT_EQUAL((int)Screen::Inbox, (int)f.ui.screen());
}
void test_ui_read_and_reply_flow() {
  UiFixture f;
  f.svc.onReceive(dataFrom(PEER, "Meet at the north ridge node at noon please", 1, ml::AddressMode::Broadcast), -90, 50);
  f.ui.notifyIncoming(50);
  f.ui.render(60);
  TEST_ASSERT_TRUE(f.cv.has("MSG 1"));
  TEST_ASSERT_TRUE(f.cv.has("New message"));
  f.swipe(Gesture::SwipeLeft, 100);
  f.ui.render(110);
  TEST_ASSERT_TRUE(f.cv.has("Inbox (1)"));
  f.tap(30, 14, 200);  // first row
  TEST_ASSERT_EQUAL((int)Screen::Message, (int)f.ui.screen());
  TEST_ASSERT_EQUAL_UINT32(0, f.svc.inbox().unread());
  f.ui.render(210);
  TEST_ASSERT_TRUE(f.cv.has("From abcd1234"));
  TEST_ASSERT_TRUE(f.cv.has("Meet at the north"));
  f.tap(60, 58, 300);  // reply bar
  TEST_ASSERT_EQUAL((int)Screen::Reply, (int)f.ui.screen());
  f.tap(30, 12 + 12 * 3 + 2, 400);  // 4th phrase, "On my way"
  TEST_ASSERT_EQUAL((int)Screen::Message, (int)f.ui.screen());
  TEST_ASSERT_EQUAL_UINT32(1, f.svc.pending());
  const OutItem& o = f.svc.outbox(0);
  TEST_ASSERT_EQUAL((int)ml::AddressMode::Direct, (int)o.packet.mode);
  TEST_ASSERT_EQUAL_MEMORY(PEER, o.packet.destination, 8);
  char text[MAX_TEXT + 1];
  decodeMessage(o.packet.payload, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("On my way", text);
  f.ui.render(410);
  TEST_ASSERT_TRUE(f.cv.has("Queued"));
}
void test_ui_reply_scroll_reaches_last_phrase() {
  UiFixture f;
  f.svc.onReceive(dataFrom(PEER, "hi", 1, ml::AddressMode::Broadcast), -90, 0);
  f.swipe(Gesture::SwipeLeft, 10);
  f.tap(30, 14, 20);
  f.tap(60, 58, 30);
  f.swipe(Gesture::SwipeUp, 40);
  f.swipe(Gesture::SwipeUp, 50);
  f.swipe(Gesture::SwipeUp, 60);  // clamps
  f.ui.render(70);
  TEST_ASSERT_TRUE(f.cv.has("Call me"));
  f.tap(30, 12 + 12 * 3 + 2, 80);  // last visible row = phrase 6
  char text[MAX_TEXT + 1];
  decodeMessage(f.svc.outbox(0).packet.payload, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Call me", text);
}
void test_ui_asleep_render_draws_nothing() {
  UiFixture f;
  f.ui.render(Ui::SLEEP_AFTER_MS + 5);
  TEST_ASSERT_EQUAL_UINT32(0, f.cv.texts.size());
  TEST_ASSERT_EQUAL(1, f.cv.presents);
}

// ---- console -----------------------------------------------------------------
struct ConFixture : UiFixture {
  DeviceInfo dev;
  ConsoleContext ctx{&svc, &ui, &clock, &dev};
  std::string run(const char* line, uint32_t t = 1000) { return handleCommand(line, ctx, t); }
};
static void injectToUi(void* user, Gesture g, int16_t x, int16_t y) {
  static_cast<Ui*>(user)->onGesture({g, x, y}, 1000);
}
void test_console_basics() {
  ConFixture f;
  f.dev.radioState = "ready";
  TEST_ASSERT_TRUE(f.run("status").find("\"rfEnabled\":false") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("bogus").find("unknown_command") != std::string::npos);
  TEST_ASSERT_EQUAL_STRING("", f.run("   ").c_str());
  TEST_ASSERT_TRUE(f.run("send zz hi").find("bad_args") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("send 4d4c090900000001 hello").find("queued") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT32(1, f.svc.pending());
  TEST_ASSERT_TRUE(f.run("broadcast ").find("rejected") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("time 1000 -480").find("\"set\":true") != std::string::npos);
  TEST_ASSERT_TRUE(f.clock.valid());
  TEST_ASSERT_TRUE(f.run("time abc").find("bad_args") != std::string::npos);
}
void test_console_sim_inbox_and_gestures() {
  ConFixture f;
  TEST_ASSERT_TRUE(f.run("tap 1 1").find("unavailable") != std::string::npos);
  f.ctx.injectGesture = injectToUi;
  f.ctx.user = &f.ui;
  TEST_ASSERT_TRUE(f.run("sim say \"hi\" \\ there").find("stored") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT32(1, f.svc.inbox().unread());
  std::string inbox = f.run("inbox");
  TEST_ASSERT_TRUE(inbox.find("say \\\"hi\\\" \\\\ there") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("swipe left", 1000).find("\"inbox\"") != std::string::npos);
  TEST_ASSERT_TRUE(f.run("swipe sideways").find("bad_args") != std::string::npos);
}
void test_parse_node_id() {
  uint8_t id[8];
  TEST_ASSERT_TRUE(parseNodeId("4D4c0102030405ff", id));
  TEST_ASSERT_EQUAL_UINT8(0x4d, id[0]);
  TEST_ASSERT_EQUAL_UINT8(0xff, id[7]);
  TEST_ASSERT_FALSE(parseNodeId("4d4c01", id));
  TEST_ASSERT_FALSE(parseNodeId("4d4c0102030405fg", id));
  TEST_ASSERT_FALSE(parseNodeId(nullptr, id));
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
  RUN_TEST(test_gesture_classification);
  RUN_TEST(test_clock);
  RUN_TEST(test_wrap_text);
  RUN_TEST(test_ui_navigation_and_wake);
  RUN_TEST(test_ui_read_and_reply_flow);
  RUN_TEST(test_ui_reply_scroll_reaches_last_phrase);
  RUN_TEST(test_ui_asleep_render_draws_nothing);
  RUN_TEST(test_console_basics);
  RUN_TEST(test_console_sim_inbox_and_gestures);
  RUN_TEST(test_parse_node_id);
  RUN_TEST(test_watch_router_delivers_direct_and_drops_transit);
  return UNITY_END();
}
