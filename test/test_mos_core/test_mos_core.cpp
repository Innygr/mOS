#include <unity.h>
#include <string>
#include "rec_canvas.h"
#include "mos_console.h"
#include "mos_shell.h"
#include "mos_radio_power.h"
#include "mos_keyboard.h"

using namespace mos;

// A minimal app proves the OS works with no Mission Link in the picture.
struct DummyApp : App {
  const char* label;
  int badgeCount = 0, ticks = 0, opened = 0;
  bool consumeAll = false;
  std::vector<Gesture> seen;
  explicit DummyApp(const char* n) : label(n) {}
  const char* name() const override { return label; }
  const char* shortName() const override { return "DUM"; }
  int badge() const override { return badgeCount; }
  void onOpen(uint32_t) override { ++opened; }
  void tick(uint32_t) override { ++ticks; }
  bool onGesture(const GestureEvent& ev, uint32_t) override { seen.push_back(ev.gesture); return consumeAll; }
  void render(Canvas& c, uint32_t) override { c.drawText(0, 0, label); }
  bool onPacket(const ml::Packet&, int16_t, uint32_t) override { return badgeCount == 42; }
  bool onCommand(const std::string& cmd, const std::string&, uint32_t, std::string& reply) override {
    if (cmd != "ping") return false;
    reply = "{\"type\":\"pong\"}";
    return true;
  }
  const char* commandUsage() const override { return "\"ping\""; }
};
struct NullSink : IPacketSink {
  TxResult transmit(const ml::Packet&) override { return TxResult::Blocked; }
};
struct Fixture {
  RecCanvas cv;
  WatchClock clock;
  DeviceInfo dev;
  NullSink radio;
  Shell shell{cv, clock, dev, radio};
  DummyApp a{"Alpha"}, b{"Beta"};
  Fixture() { shell.addApp(&a); shell.addApp(&b); }
  void g(Gesture gg, uint32_t t, int x = 0, int y = 0) { shell.onGesture(GestureEvent(gg, (int16_t)x, (int16_t)y), t); }
};

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

void test_shell_navigation_and_app_lifecycle() {
  Fixture f;
  f.shell.render(100);
  TEST_ASSERT_TRUE(f.cv.has("--:--"));
  f.g(Gesture::SwipeLeft, 200);
  TEST_ASSERT_EQUAL((int)View::Launcher, (int)f.shell.view());
  f.shell.render(210);
  TEST_ASSERT_TRUE(f.cv.has("Alpha"));
  TEST_ASSERT_TRUE(f.cv.has("Beta"));
  f.g(Gesture::Tap, 300, 30, 12 + 12 + 2);          // second row
  TEST_ASSERT_EQUAL((int)View::App, (int)f.shell.view());
  TEST_ASSERT_EQUAL_STRING("Beta", f.shell.running()->name());
  TEST_ASSERT_EQUAL(1, f.b.opened);
  f.g(Gesture::SwipeRight, 400);                    // unhandled -> launcher
  TEST_ASSERT_EQUAL((int)View::Launcher, (int)f.shell.view());
  f.g(Gesture::Tap, 500, 30, 14);
  f.g(Gesture::LongPress, 600);                     // unhandled long press -> home
  TEST_ASSERT_EQUAL((int)View::Face, (int)f.shell.view());
  TEST_ASSERT_NULL(f.shell.running());
}
void test_app_can_consume_gestures() {
  Fixture f;
  f.shell.open("Alpha", 0);
  f.a.consumeAll = true;
  f.g(Gesture::SwipeRight, 10);
  TEST_ASSERT_EQUAL((int)View::App, (int)f.shell.view());
  TEST_ASSERT_EQUAL_UINT32(1, f.a.seen.size());
}
void test_background_tick_and_packet_dispatch() {
  Fixture f;
  f.shell.tick(1); f.shell.tick(2);
  TEST_ASSERT_EQUAL(2, f.a.ticks);
  TEST_ASSERT_EQUAL(2, f.b.ticks);        // not foreground, still ticks
  ml::Packet p;
  TEST_ASSERT_FALSE(f.shell.dispatchPacket(p, 0, 0));
  f.b.badgeCount = 42;
  TEST_ASSERT_TRUE(f.shell.dispatchPacket(p, 0, 0));
}
void test_launcher_scroll_and_app_limit() {
  Fixture f;
  DummyApp c("C"), d("D"), e("E"), h("H");
  for (App* x : {(App*)&c, (App*)&d, (App*)&e, (App*)&h}) TEST_ASSERT_TRUE(f.shell.addApp(x));
  f.g(Gesture::SwipeLeft, 10);
  f.g(Gesture::SwipeUp, 20); f.g(Gesture::SwipeUp, 30); f.g(Gesture::SwipeUp, 40);
  f.shell.render(50);
  TEST_ASSERT_FALSE(f.cv.has("Alpha"));
  TEST_ASSERT_TRUE(f.cv.has("H"));
  DummyApp x1("1"), x2("2");
  TEST_ASSERT_TRUE(f.shell.addApp(&x1));
  TEST_ASSERT_TRUE(f.shell.addApp(&x2));
  TEST_ASSERT_FALSE(f.shell.addApp(&x2));  // MAX_APPS reached
}
void test_face_badges_and_notifications() {
  Fixture f;
  f.a.badgeCount = 3;
  f.shell.notify("Hello", 100);
  f.shell.render(110);
  TEST_ASSERT_TRUE(f.cv.has("DUM 3"));
  TEST_ASSERT_TRUE(f.cv.has("Hello"));
  f.shell.render(100 + Shell::TOAST_MS + 1);
  TEST_ASSERT_FALSE(f.cv.has("Hello"));
  f.dev.receiving = true;
  f.shell.render(2000);
  TEST_ASSERT_TRUE(f.cv.has("RX"));
}
void test_sleep_and_wake() {
  Fixture f;
  f.g(Gesture::SwipeLeft, 100);
  uint32_t later = 100 + Shell::SLEEP_AFTER_MS + 1;
  TEST_ASSERT_TRUE(f.shell.asleep(later));
  f.shell.render(later);
  TEST_ASSERT_EQUAL_UINT32(0, f.cv.texts.size());
  f.g(Gesture::SwipeLeft, later);        // wakes, goes home, does not navigate
  TEST_ASSERT_EQUAL((int)View::Face, (int)f.shell.view());
  TEST_ASSERT_FALSE(f.shell.asleep(later));
  f.shell.notify("x", later + Shell::SLEEP_AFTER_MS + 5);  // a notification also wakes
  TEST_ASSERT_FALSE(f.shell.asleep(later + Shell::SLEEP_AFTER_MS + 5));
}
void test_lora_symbol_time_and_duty_plan() {
  // SF9 / 125 kHz: 512 / 125 = 4.096 ms per symbol.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.096f, loraSymbolMs(9, 125.0f));
  TEST_ASSERT_EQUAL_FLOAT(0, loraSymbolMs(4, 125.0f));
  TEST_ASSERT_EQUAL_FLOAT(0, loraSymbolMs(9, 0));
  const DutyCyclePlan p = planDutyCycle(9, 125.0f, 1000);   // sleep 1 s between listens
  TEST_ASSERT_TRUE(p.valid);
  TEST_ASSERT_EQUAL_UINT16(245 + 4, p.senderPreambleSymbols);  // ceil(1000/4.096)=245
  TEST_ASSERT_FLOAT_WITHIN(0.5f, (249 - 8) * 4.096f, p.extraAirtimeMs);  // ~987 ms longer frames
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 4 * 4.096f / (4 * 4.096f + 1000.0f), p.listenFraction);
  TEST_ASSERT_TRUE(p.listenFraction < 0.02f);                 // under 2% awake
  // Shorter sleep: cheaper for senders, costlier for the receiver.
  const DutyCyclePlan q = planDutyCycle(9, 125.0f, 100);
  TEST_ASSERT_TRUE(q.senderPreambleSymbols < p.senderPreambleSymbols);
  TEST_ASSERT_TRUE(q.listenFraction > p.listenFraction);
  // Invalid inputs.
  TEST_ASSERT_FALSE(planDutyCycle(9, 125.0f, 0).valid);
  TEST_ASSERT_FALSE(planDutyCycle(9, 125.0f, 1000, 1).valid);
  TEST_ASSERT_FALSE(planDutyCycle(13, 125.0f, 1000).valid);
  TEST_ASSERT_FALSE(planDutyCycle(12, 7.8f, 60000000).valid);   // preamble would overflow 16 bits
  // Tiny sleep never reports negative extra airtime.
  TEST_ASSERT_TRUE(planDutyCycle(9, 125.0f, 1, 2).extraAirtimeMs >= 0);
}

void test_console_system_commands() {
  Fixture f;
  auto run = [&](const char* l, uint32_t t = 1000) { return handleCommand(l, f.shell, f.clock, f.dev, t); };
  TEST_ASSERT_TRUE(run("help").find("\"ping\"") != std::string::npos);
  TEST_ASSERT_TRUE(run("status").find("\"rfEnabled\":false") != std::string::npos);
  TEST_ASSERT_TRUE(run("apps").find("Alpha") != std::string::npos);
  TEST_ASSERT_TRUE(run("open Nope").find("no_such_app") != std::string::npos);
  TEST_ASSERT_TRUE(run("open Alpha").find("opened") != std::string::npos);
  TEST_ASSERT_EQUAL((int)View::App, (int)f.shell.view());
  TEST_ASSERT_TRUE(run("home").find("home") != std::string::npos);
  TEST_ASSERT_TRUE(run("swipe left").find("launcher") != std::string::npos);
  TEST_ASSERT_TRUE(run("swipe sideways").find("bad_args") != std::string::npos);
  TEST_ASSERT_TRUE(run("tap 1").find("bad_args") != std::string::npos);
  TEST_ASSERT_TRUE(run("time 1000 -480").find("\"set\":true") != std::string::npos);
  TEST_ASSERT_TRUE(f.clock.valid());
  TEST_ASSERT_TRUE(run("time abc").find("bad_args") != std::string::npos);
  TEST_ASSERT_TRUE(run("ping").find("pong") != std::string::npos);   // routed to an app
  TEST_ASSERT_TRUE(run("bogus").find("unknown_command") != std::string::npos);
  TEST_ASSERT_EQUAL_STRING("", run("   ").c_str());
}


void test_shared_text_keyboard() {
  RecCanvas cv;
  TextKeyboard kb;
  kb.begin("Hi");
  TEST_ASSERT_TRUE(kb.active());
  TEST_ASSERT_EQUAL_STRING("Hi", kb.value());
  kb.render(cv, "Wi-Fi");
  KeyboardResult result = KeyboardResult::None;
  // First key in the top letter row is q.
  TEST_ASSERT_TRUE(kb.onTap(1, 18, result));
  TEST_ASSERT_EQUAL((int)KeyboardResult::Changed, (int)result);
  TEST_ASSERT_EQUAL_STRING("Hiq", kb.value());
  // Shift toggles uppercase; next top-row key is Q.
  TEST_ASSERT_TRUE(kb.onTap(1, 37, result));
  TEST_ASSERT_TRUE(kb.uppercase());
  TEST_ASSERT_TRUE(kb.onTap(1, 18, result));
  TEST_ASSERT_EQUAL_STRING("HiqQ", kb.value());
  // Confirm is the rightmost footer segment.
  TEST_ASSERT_TRUE(kb.onTap(cv.width() - 1, cv.height() - 1, result));
  TEST_ASSERT_EQUAL((int)KeyboardResult::Confirm, (int)result);
  TEST_ASSERT_FALSE(kb.active());
}

void test_shared_text_keyboard_cancel() {
  RecCanvas cv;
  TextKeyboard kb;
  kb.begin("draft");
  kb.render(cv);
  KeyboardResult result = KeyboardResult::None;
  TEST_ASSERT_TRUE(kb.onTap(1, cv.height() - 1, result));
  TEST_ASSERT_EQUAL((int)KeyboardResult::Cancel, (int)result);
  TEST_ASSERT_EQUAL_STRING("draft", kb.value());
  TEST_ASSERT_FALSE(kb.active());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_gesture_classification);
  RUN_TEST(test_shared_text_keyboard);
  RUN_TEST(test_shared_text_keyboard_cancel);
  RUN_TEST(test_clock);
  RUN_TEST(test_wrap_text);
  RUN_TEST(test_shell_navigation_and_app_lifecycle);
  RUN_TEST(test_app_can_consume_gestures);
  RUN_TEST(test_background_tick_and_packet_dispatch);
  RUN_TEST(test_launcher_scroll_and_app_limit);
  RUN_TEST(test_face_badges_and_notifications);
  RUN_TEST(test_sleep_and_wake);
  RUN_TEST(test_lora_symbol_time_and_duty_plan);
  RUN_TEST(test_console_system_commands);
  return UNITY_END();
}
