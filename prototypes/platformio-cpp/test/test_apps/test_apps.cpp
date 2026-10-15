#include <unity.h>
#include "rec_canvas.h"
#include "mos_shell.h"
#include "mos_store.h"
#include "settings_app.h"
#include "stopwatch_app.h"

using namespace mos;

struct NullSink : IPacketSink {
  TxResult transmit(const ml::Packet&) override { return TxResult::Blocked; }
};
struct Fixture {
  RecCanvas cv;
  WatchClock clock;
  DeviceInfo dev;
  NullSink radio;
  MemStore store;
  Shell shell{cv, clock, dev, radio, &store};
};
static GestureEvent ev(Gesture g, int x = 0, int y = 0) { return GestureEvent(g, (int16_t)x, (int16_t)y); }

void test_stopwatch_start_stop_resume_reset() {
  StopwatchApp sw;
  TEST_ASSERT_EQUAL_UINT32(0, sw.elapsedMs(500));
  TEST_ASSERT_TRUE(sw.onGesture(ev(Gesture::Tap), 1000));      // start
  TEST_ASSERT_EQUAL_UINT32(2500, sw.elapsedMs(3500));
  TEST_ASSERT_TRUE(sw.onGesture(ev(Gesture::Tap), 4000));      // stop at 3000
  TEST_ASSERT_EQUAL_UINT32(3000, sw.elapsedMs(99999));
  sw.onGesture(ev(Gesture::Tap), 10000);                       // resume
  TEST_ASSERT_EQUAL_UINT32(4000, sw.elapsedMs(11000));
  TEST_ASSERT_TRUE(sw.onGesture(ev(Gesture::LongPress), 12000));
  TEST_ASSERT_FALSE(sw.running());
  TEST_ASSERT_EQUAL_UINT32(0, sw.elapsedMs(13000));
  TEST_ASSERT_FALSE(sw.onGesture(ev(Gesture::SwipeRight), 0));  // left to the OS
}
void test_stopwatch_survives_millis_wrap_and_renders() {
  Fixture f;
  StopwatchApp sw;
  f.shell.addApp(&sw);
  sw.onGesture(ev(Gesture::Tap), 0xFFFFFF00u);
  TEST_ASSERT_EQUAL_UINT32(0x200u, sw.elapsedMs(0x100u));
  f.shell.open("Stopwatch", 0);
  f.shell.render(1);
  TEST_ASSERT_TRUE(f.cv.has("Stopwatch"));
  TEST_ASSERT_TRUE(f.cv.has("tap: stop"));
}
void test_settings_adjust_apply_and_persist() {
  Fixture f;
  SettingsApp set;
  f.shell.addApp(&set);
  TEST_ASSERT_EQUAL_UINT32(Shell::SLEEP_AFTER_MS, f.shell.sleepAfterMs());
  f.shell.open("Settings", 0);
  for (int i = 0; i < 16; ++i) set.onGesture(ev(Gesture::Tap, 10, 12 + 2), 0);  // -8:00
  f.clock.setEpoch(12 * 3600, 0);
  uint8_t h, m, s;
  f.clock.timeOfDay(0, h, m, s);
  TEST_ASSERT_EQUAL_UINT8(4, h);
  set.onGesture(ev(Gesture::Tap, 10, 12 + 12 + 2), 0);   // 15 s -> 30 s
  TEST_ASSERT_EQUAL_UINT32(30000, f.shell.sleepAfterMs());
  uint32_t v;
  TEST_ASSERT_TRUE(f.store.getU32(SettingsApp::KEY_SLEEP_MS, v));
  TEST_ASSERT_EQUAL_UINT32(30000, v);
  f.shell.render(5);
  TEST_ASSERT_TRUE(f.cv.has("UTC -8:00"));
  TEST_ASSERT_TRUE(f.cv.has("Screen off: 30s"));

  // A fresh boot with the same store restores both settings.
  Fixture g;
  g.store = f.store;
  Shell s2(g.cv, g.clock, g.dev, g.radio, &g.store);
  SettingsApp set2;
  s2.addApp(&set2);
  TEST_ASSERT_EQUAL_UINT32(30000, s2.sleepAfterMs());
  g.clock.setEpoch(12 * 3600, 0);
  g.clock.timeOfDay(0, h, m, s);
  TEST_ASSERT_EQUAL_UINT8(4, h);
}
void test_settings_limits_corrupt_store_and_no_store() {
  Fixture f;
  f.store.putU32(SettingsApp::KEY_UTC_OFFSET, 5000);   // out of range
  f.store.putU32(SettingsApp::KEY_SLEEP_MS, 7);        // not an allowed value
  SettingsApp set;
  f.shell.addApp(&set);
  TEST_ASSERT_EQUAL_UINT32(Shell::SLEEP_AFTER_MS, f.shell.sleepAfterMs());
  for (int i = 0; i < 40; ++i) set.onGesture(ev(Gesture::Tap, 100, 14), 0);  // push offset up
  f.clock.setEpoch(0, 0);
  uint8_t h, m, s;
  f.clock.timeOfDay(0, h, m, s);
  TEST_ASSERT_EQUAL_UINT8(14, h);                      // clamped at UTC+14
  // Without any store the app still works.
  RecCanvas cv; WatchClock clock; DeviceInfo dev; NullSink radio;
  Shell bare(cv, clock, dev, radio);
  SettingsApp set2;
  bare.addApp(&set2);
  TEST_ASSERT_TRUE(set2.onGesture(ev(Gesture::Tap, 10, 14), 0));
}
void test_shell_sleep_timeout_has_floor() {
  Fixture f;
  f.shell.setSleepAfterMs(1);
  TEST_ASSERT_EQUAL_UINT32(3000, f.shell.sleepAfterMs());
  f.shell.setSleepAfterMs(60000);
  TEST_ASSERT_FALSE(f.shell.asleep(59000));
  TEST_ASSERT_TRUE(f.shell.asleep(60000));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_stopwatch_start_stop_resume_reset);
  RUN_TEST(test_stopwatch_survives_millis_wrap_and_renders);
  RUN_TEST(test_settings_adjust_apply_and_persist);
  RUN_TEST(test_settings_limits_corrupt_store_and_no_store);
  RUN_TEST(test_shell_sleep_timeout_has_floor);
  return UNITY_END();
}
