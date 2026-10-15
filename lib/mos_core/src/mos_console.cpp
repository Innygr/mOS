#include "mos_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace mos {
static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool parseNodeId(const char* hex, uint8_t out[8]) {
  if (!hex || strlen(hex) != 16) return false;
  for (int i = 0; i < 8; ++i) {
    const int hi = hexVal(hex[2 * i]), lo = hexVal(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<uint8_t>(hi << 4 | lo);
  }
  return true;
}

static std::string err(const char* code, const char* msg) {
  return std::string("{\"type\":\"error\",\"code\":\"") + code + "\",\"message\":\"" + msg + "\"}";
}

static const char* screenName(Screen s) {
  switch (s) {
    case Screen::Face: return "face";
    case Screen::Inbox: return "inbox";
    case Screen::Message: return "message";
    case Screen::Reply: return "reply";
    case Screen::Status: return "status";
  }
  return "unknown";
}

std::string handleCommand(const std::string& rawLine, ConsoleContext& ctx, uint32_t nowMs) {
  std::string line = rawLine;
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
  while (!line.empty() && line.front() == ' ') line.erase(0, 1);
  if (line.empty()) return "";
  const size_t sp = line.find(' ');
  const std::string cmd = line.substr(0, sp);
  const std::string rest = sp == std::string::npos ? "" : line.substr(sp + 1);
  char buf[160];

  if (cmd == "help") {
    return "{\"type\":\"help\",\"commands\":[\"help\",\"status\",\"inbox\",\"send <node-id-hex16> <text>\","
           "\"broadcast <text>\",\"time <epoch-seconds> [utc-offset-minutes]\","
           "\"sim <text>\",\"tap <x> <y>\",\"swipe <left|right|up|down>\"]}";
  }
  if (cmd == "status") {
    snprintf(buf, sizeof(buf),
             "{\"type\":\"status\",\"firmware\":\"%s\",\"rfEnabled\":%s,\"radioState\":\"%s\","
             "\"preset\":\"%s\",\"screen\":\"%s\",\"unread\":%u,\"outboxPending\":%u,\"txHeld\":%s}",
#ifdef MOS_VERSION
             MOS_VERSION,
#else
             "native",
#endif
             ctx.dev->rfEnabled ? "true" : "false", ctx.dev->radioState, ctx.dev->preset,
             screenName(ctx.ui->screen()), static_cast<unsigned>(ctx.svc->inbox().unread()),
             static_cast<unsigned>(ctx.svc->pending()), ctx.svc->txBlocked() ? "true" : "false");
    return buf;
  }
  if (cmd == "inbox") {
    std::string out = "{\"type\":\"inbox\",\"items\":[";
    for (size_t i = 0; i < ctx.svc->inbox().count(); ++i) {
      const InboxItem* it = ctx.svc->inbox().at(i);
      char id[17];
      for (int b = 0; b < 8; ++b) snprintf(id + 2 * b, 3, "%02x", it->from[b]);
      if (i) out += ",";
      std::string text;  // text is printable ASCII; escape what would break JSON
      for (const char* c = it->text; *c; ++c) {
        if (*c == '"' || *c == '\\') text += '\\';
        text += *c;
      }
      out += std::string("{\"from\":\"") + id + "\",\"read\":" + (it->read ? "true" : "false") +
             ",\"text\":\"" + text + "\"}";
    }
    return out + "]}";
  }
  if (cmd == "send") {
    const size_t sp2 = rest.find(' ');
    uint8_t dest[8];
    if (sp2 == std::string::npos || !parseNodeId(rest.substr(0, sp2).c_str(), dest))
      return err("bad_args", "Usage: send <16-hex-char node id> <text>");
    if (!ctx.svc->sendText(dest, rest.substr(sp2 + 1).c_str(), nowMs))
      return err("rejected", "Text empty, too long, or outbox full.");
    return "{\"type\":\"queued\"}";
  }
  if (cmd == "broadcast") {
    if (!ctx.svc->broadcastText(rest.c_str(), nowMs))
      return err("rejected", "Text empty, too long, or outbox full.");
    return "{\"type\":\"queued\"}";
  }
  if (cmd == "time") {
    char* end = nullptr;
    const unsigned long epoch = strtoul(rest.c_str(), &end, 10);
    if (end == rest.c_str()) return err("bad_args", "Usage: time <epoch-seconds> [utc-offset-minutes]");
    ctx.clock->setEpoch(static_cast<uint32_t>(epoch), nowMs);
    while (*end == ' ') ++end;
    if (*end) ctx.clock->setUtcOffsetMinutes(static_cast<int16_t>(strtol(end, nullptr, 10)));
    return "{\"type\":\"time\",\"set\":true}";
  }
  if (cmd == "sim") {
    // Injected locally; this does not exercise the radio or router.
    static const uint8_t simId[8] = {'M', 'L', 0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    std::vector<uint8_t> body;
    ml::Packet p;
    if (!encodeText(rest.c_str(), body)) return err("rejected", "Text empty or too long.");
    p.mode = ml::AddressMode::Broadcast;  // broadcast: no ack is queued for a simulated sender
    memcpy(p.source, simId, 8);
    p.messageId[7] = 1;
    p.payload = body;
    ctx.svc->onReceive(p, -90, nowMs);
    ctx.ui->notifyIncoming(nowMs);
    return "{\"type\":\"sim\",\"stored\":true}";
  }
  if ((cmd == "tap" || cmd == "swipe") && !ctx.injectGesture)
    return err("unavailable", "No gesture injector attached.");
  if (cmd == "tap") {
    int x = 0, y = 0;
    if (sscanf(rest.c_str(), "%d %d", &x, &y) != 2) return err("bad_args", "Usage: tap <x> <y>");
    ctx.injectGesture(ctx.user, Gesture::Tap, static_cast<int16_t>(x), static_cast<int16_t>(y));
    return std::string("{\"type\":\"screen\",\"name\":\"") + screenName(ctx.ui->screen()) + "\"}";
  }
  if (cmd == "swipe") {
    Gesture g = Gesture::None;
    if (rest == "left") g = Gesture::SwipeLeft;
    else if (rest == "right") g = Gesture::SwipeRight;
    else if (rest == "up") g = Gesture::SwipeUp;
    else if (rest == "down") g = Gesture::SwipeDown;
    else return err("bad_args", "Usage: swipe <left|right|up|down>");
    ctx.injectGesture(ctx.user, g, 0, 0);
    return std::string("{\"type\":\"screen\",\"name\":\"") + screenName(ctx.ui->screen()) + "\"}";
  }
  return err("unknown_command", "Unknown command. Send help for available commands.");
}
}  // namespace mos
