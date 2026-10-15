#include "mos_console.h"
#include <stdio.h>
#include <stdlib.h>

namespace mos {
static std::string err(const char* code, const char* msg) {
  return std::string("{\"type\":\"error\",\"code\":\"") + code + "\",\"message\":\"" + msg + "\"}";
}
static const char* viewName(View v) {
  switch (v) {
    case View::Face: return "face";
    case View::Launcher: return "launcher";
    case View::App: return "app";
  }
  return "unknown";
}

std::string handleCommand(const std::string& rawLine, Shell& shell, WatchClock& clock,
                          const DeviceInfo& dev, uint32_t nowMs) {
  std::string line = rawLine;
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
  while (!line.empty() && line.front() == ' ') line.erase(0, 1);
  if (line.empty()) return "";
  const size_t sp = line.find(' ');
  const std::string cmd = line.substr(0, sp);
  const std::string rest = sp == std::string::npos ? "" : line.substr(sp + 1);

  if (cmd == "help") {
    std::string out =
        "{\"type\":\"help\",\"commands\":[\"help\",\"status\",\"apps\",\"open <app>\",\"home\","
        "\"time <epoch-seconds> [utc-offset-minutes]\",\"tap <x> <y>\",\"swipe <left|right|up|down>\"";
    for (size_t i = 0; i < shell.appCount(); ++i) {
      const char* u = shell.appAt(i)->commandUsage();
      if (*u) out += std::string(",") + u;
    }
    return out + "]}";
  }
  if (cmd == "status") {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"status\",\"firmware\":\"%s\",\"rfEnabled\":%s,\"radioState\":\"%s\","
             "\"preset\":\"%s\",\"view\":\"%s\",\"app\":\"%s\"}",
#ifdef MOS_VERSION
             MOS_VERSION,
#else
             "native",
#endif
             dev.rfEnabled ? "true" : "false", dev.radioState, dev.preset, viewName(shell.view()),
             shell.running() ? shell.running()->name() : "");
    return buf;
  }
  if (cmd == "apps") {
    std::string out = "{\"type\":\"apps\",\"items\":[";
    for (size_t i = 0; i < shell.appCount(); ++i) {
      if (i) out += ",";
      out += std::string("{\"name\":\"") + shell.appAt(i)->name() +
             "\",\"badge\":" + std::to_string(shell.appAt(i)->badge()) + "}";
    }
    return out + "]}";
  }
  if (cmd == "open") {
    if (!shell.open(rest.c_str(), nowMs)) return err("no_such_app", "Send apps for the list.");
    return std::string("{\"type\":\"opened\",\"app\":\"") + rest + "\"}";
  }
  if (cmd == "home") {
    shell.goHome();
    return "{\"type\":\"home\"}";
  }
  if (cmd == "time") {
    char* end = nullptr;
    const unsigned long epoch = strtoul(rest.c_str(), &end, 10);
    if (end == rest.c_str()) return err("bad_args", "Usage: time <epoch-seconds> [utc-offset-minutes]");
    clock.setEpoch(static_cast<uint32_t>(epoch), nowMs);
    while (*end == ' ') ++end;
    if (*end) clock.setUtcOffsetMinutes(static_cast<int16_t>(strtol(end, nullptr, 10)));
    return "{\"type\":\"time\",\"set\":true}";
  }
  if (cmd == "tap") {
    int x = 0, y = 0;
    if (sscanf(rest.c_str(), "%d %d", &x, &y) != 2) return err("bad_args", "Usage: tap <x> <y>");
    shell.onGesture(GestureEvent(Gesture::Tap, static_cast<int16_t>(x), static_cast<int16_t>(y)), nowMs);
    return std::string("{\"type\":\"view\",\"name\":\"") + viewName(shell.view()) + "\"}";
  }
  if (cmd == "swipe") {
    Gesture g = Gesture::None;
    if (rest == "left") g = Gesture::SwipeLeft;
    else if (rest == "right") g = Gesture::SwipeRight;
    else if (rest == "up") g = Gesture::SwipeUp;
    else if (rest == "down") g = Gesture::SwipeDown;
    else return err("bad_args", "Usage: swipe <left|right|up|down>");
    shell.onGesture(GestureEvent(g), nowMs);
    return std::string("{\"type\":\"view\",\"name\":\"") + viewName(shell.view()) + "\"}";
  }
  std::string reply;
  for (size_t i = 0; i < shell.appCount(); ++i)
    if (shell.appAt(i)->onCommand(cmd, rest, nowMs, reply)) return reply;
  return err("unknown_command", "Unknown command. Send help for available commands.");
}
}  // namespace mos
