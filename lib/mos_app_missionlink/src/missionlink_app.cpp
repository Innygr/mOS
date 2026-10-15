#include "missionlink_app.h"
#include "mos_layout.h"
#include <stdio.h>
#include <string.h>

namespace mos {
// 16 hex chars -> 8 bytes.
static bool parseNodeId(const char* hex, uint8_t out[8]) {
  if (!hex || strlen(hex) != 16) return false;
  for (int i = 0; i < 8; ++i) {
    int v[2];
    for (int k = 0; k < 2; ++k) {
      const char c = hex[2 * i + k];
      v[k] = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10
           : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
      if (v[k] < 0) return false;
    }
    out[i] = static_cast<uint8_t>(v[0] << 4 | v[1]);
  }
  return true;
}

static void shortId(const uint8_t id[8], char out[9]) {
  // The last 4 bytes of the node id (the tail of the MAC) are the readable part.
  static const char* hex = "0123456789abcdef";
  for (int i = 0; i < 4; ++i) {
    out[2 * i] = hex[id[4 + i] >> 4];
    out[2 * i + 1] = hex[id[4 + i] & 0x0f];
  }
  out[8] = '\0';
}

MissionLinkApp::MissionLinkApp(const uint8_t localId[8], uint32_t bootNonce,
                               const MessagingService::Config& cfg)
    : svc_(localId, bootNonce, cfg) {}

void MissionLinkApp::onOpen(uint32_t) { go(Page::Inbox); }

void MissionLinkApp::onStart() {
  if (!sys().store) return;
  uint8_t buf[Inbox::MAX_SERIALIZED];
  const size_t n = sys().store->getBlob(KEY_INBOX, buf, sizeof(buf));
  if (n) svc_.inbox().deserialize(buf, n);  // a corrupt blob is ignored: start empty
  savedRevision_ = svc_.inbox().revision();
}

void MissionLinkApp::persistIfDue(uint32_t nowMs) {
  if (!sys().store) return;
  const uint32_t rev = svc_.inbox().revision();
  if (rev == savedRevision_) { changePending_ = false; return; }
  if (!changePending_) { changePending_ = true; changedAtMs_ = nowMs; return; }
  if (nowMs - changedAtMs_ < SAVE_DELAY_MS) return;
  uint8_t buf[Inbox::MAX_SERIALIZED];
  const size_t n = svc_.inbox().serialize(buf, sizeof(buf));
  if (n && sys().store->putBlob(KEY_INBOX, buf, n)) savedRevision_ = rev;
  changePending_ = false;
}

void MissionLinkApp::tick(uint32_t nowMs) {
  if (sys().radio) svc_.tick(nowMs, *sys().radio);
  persistIfDue(nowMs);
}

bool MissionLinkApp::onPacket(const ml::Packet& p, int16_t rssi, uint32_t nowMs) {
  const auto outcome = svc_.onReceive(p, rssi, nowMs);
  if (outcome == MessagingService::RxOutcome::Stored && sys().notifier)
    sys().notifier->notify("New message", nowMs);
  return outcome != MessagingService::RxOutcome::Ignored;
}

int MissionLinkApp::rows() const { return lastCanvas_ ? visibleRows(*lastCanvas_) : 0; }

int MissionLinkApp::messageTextRows() const {
  // Header on top, a reply bar at the bottom.
  return lastCanvas_ ? (lastCanvas_->height() - headerH() - rowH()) / FONT_H : 0;
}

std::vector<std::string> MissionLinkApp::messageLines(const Canvas& c) const {
  const InboxItem* it = svc_.inbox().at(selected_);
  return it ? wrapText(it->text, textCols(c)) : std::vector<std::string>();
}

bool MissionLinkApp::onGesture(const GestureEvent& ev, uint32_t nowMs) {
  if (!lastCanvas_) return false;  // nothing drawn yet, so no layout to hit-test
  const Canvas& c = *lastCanvas_;
  switch (page_) {
    case Page::Inbox: {
      const size_t n = svc_.inbox().count();
      if (ev.gesture == Gesture::SwipeUp) {
        if (scroll_ + static_cast<size_t>(rows()) < n) ++scroll_;
        return true;
      }
      if (ev.gesture == Gesture::SwipeDown) {
        if (scroll_ > 0) --scroll_;
        return true;
      }
      if (ev.gesture == Gesture::Tap && ev.y >= headerH()) {
        const size_t idx = scroll_ + static_cast<size_t>((ev.y - headerH()) / rowH());
        if (idx < n) {
          selected_ = idx;
          svc_.inbox().markRead(idx);
          go(Page::Message);
        }
        return true;
      }
      return false;  // swipe right etc.: let the OS close the app
    }
    case Page::Message:
      if (ev.gesture == Gesture::SwipeRight) { go(Page::Inbox); return true; }
      if (ev.gesture == Gesture::SwipeUp) {
        if (scroll_ + static_cast<size_t>(messageTextRows()) < messageLines(c).size()) ++scroll_;
        return true;
      }
      if (ev.gesture == Gesture::SwipeDown) {
        if (scroll_ > 0) --scroll_;
        return true;
      }
      if (ev.gesture == Gesture::Tap) {
        if (ev.y >= c.height() - rowH() && svc_.inbox().at(selected_)) go(Page::Reply);
        return true;
      }
      return false;
    case Page::Reply: {
      if (ev.gesture == Gesture::SwipeRight) { go(Page::Message); return true; }
      if (ev.gesture == Gesture::SwipeUp) {
        if (scroll_ + static_cast<size_t>(rows()) < CANNED_COUNT) ++scroll_;
        return true;
      }
      if (ev.gesture == Gesture::SwipeDown) {
        if (scroll_ > 0) --scroll_;
        return true;
      }
      if (ev.gesture == Gesture::Tap && ev.y >= headerH()) {
        const InboxItem* it = svc_.inbox().at(selected_);
        const int row = (ev.y - headerH()) / rowH();
        const uint8_t id = static_cast<uint8_t>(scroll_ + row + 1);
        if (it && row < rows() && cannedPhrase(id)) {
          const bool ok = svc_.sendCanned(it->from, id, nowMs);
          if (sys().notifier) sys().notifier->notify(ok ? "Queued" : "Outbox full", nowMs);
          go(Page::Message);
        }
        return true;
      }
      return false;
    }
  }
  return false;
}

void MissionLinkApp::render(Canvas& c, uint32_t) {
  lastCanvas_ = &c;
  switch (page_) {
    case Page::Inbox: renderInbox(c); break;
    case Page::Message: renderMessage(c); break;
    case Page::Reply: renderReply(c); break;
  }
}

void MissionLinkApp::renderInbox(Canvas& c) {
  char buf[MAX_TEXT + 16];
  const unsigned out = static_cast<unsigned>(svc_.pending());
  if (out) snprintf(buf, sizeof(buf), "Inbox (%u) out %u%s", static_cast<unsigned>(svc_.inbox().count()),
                    out, svc_.txBlocked() ? " held" : "");
  else snprintf(buf, sizeof(buf), "Inbox (%u)", static_cast<unsigned>(svc_.inbox().count()));
  drawHeader(c, buf);
  if (svc_.inbox().count() == 0) { c.drawText(2, headerH() + 8, "No messages"); return; }
  for (int r = 0; r < rows(); ++r) {
    const InboxItem* it = svc_.inbox().at(scroll_ + static_cast<size_t>(r));
    if (!it) break;
    char id[9];
    shortId(it->from, id);
    // "*" marks unread; sender tag then as much of the text as fits.
    snprintf(buf, sizeof(buf), "%c%.4s %s", it->read ? ' ' : '*', id, it->text);
    if (textCols(c) < static_cast<int>(sizeof(buf))) buf[textCols(c)] = '\0';
    c.drawText(0, headerH() + r * rowH() + 2, buf);
  }
}

void MissionLinkApp::renderMessage(Canvas& c) {
  const InboxItem* it = svc_.inbox().at(selected_);
  if (!it) { drawHeader(c, "Message"); return; }
  char id[9], buf[32];
  shortId(it->from, id);
  snprintf(buf, sizeof(buf), "From %s", id);
  drawHeader(c, buf);
  const std::vector<std::string> lines = messageLines(c);
  for (int r = 0; r < messageTextRows(); ++r) {
    const size_t li = scroll_ + static_cast<size_t>(r);
    if (li >= lines.size()) break;
    c.drawText(0, headerH() + 1 + r * FONT_H, lines[li].c_str());
  }
  const int by = c.height() - rowH();
  c.drawRect(0, by, c.width(), rowH());
  c.drawText(2, by + 2, "[ Reply ]");
}

void MissionLinkApp::renderReply(Canvas& c) {
  drawHeader(c, "Reply with");
  for (int r = 0; r < rows(); ++r) {
    const char* phrase = cannedPhrase(static_cast<uint8_t>(scroll_ + r + 1));
    if (!phrase) break;
    c.drawText(4, headerH() + r * rowH() + 2, phrase);
  }
}

bool MissionLinkApp::onCommand(const std::string& cmd, const std::string& rest, uint32_t nowMs,
                               std::string& reply) {
  auto err = [&](const char* code, const char* msg) {
    reply = std::string("{\"type\":\"error\",\"code\":\"") + code + "\",\"message\":\"" + msg + "\"}";
    return true;
  };
  if (cmd == "inbox") {
    std::string out = "{\"type\":\"inbox\",\"items\":[";
    for (size_t i = 0; i < svc_.inbox().count(); ++i) {
      const InboxItem* it = svc_.inbox().at(i);
      char id[17];
      for (int b = 0; b < 8; ++b) snprintf(id + 2 * b, 3, "%02x", it->from[b]);
      std::string text;  // text is printable ASCII; escape what would break JSON
      for (const char* ch = it->text; *ch; ++ch) {
        if (*ch == '"' || *ch == '\\') text += '\\';
        text += *ch;
      }
      if (i) out += ",";
      out += std::string("{\"from\":\"") + id + "\",\"read\":" + (it->read ? "true" : "false") +
             ",\"text\":\"" + text + "\"}";
    }
    reply = out + "]}";
    return true;
  }
  if (cmd == "outbox") {
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"type\":\"outbox\",\"pending\":%u,\"txHeld\":%s}",
             static_cast<unsigned>(svc_.pending()), svc_.txBlocked() ? "true" : "false");
    reply = buf;
    return true;
  }
  if (cmd == "send") {
    const size_t sp = rest.find(' ');
    uint8_t dest[8];
    if (sp == std::string::npos || !parseNodeId(rest.substr(0, sp).c_str(), dest))
      return err("bad_args", "Usage: send <16-hex-char node id> <text>");
    if (!svc_.sendText(dest, rest.substr(sp + 1).c_str(), nowMs))
      return err("rejected", "Text empty, too long, or outbox full.");
    reply = "{\"type\":\"queued\"}";
    return true;
  }
  if (cmd == "broadcast") {
    if (!svc_.broadcastText(rest.c_str(), nowMs))
      return err("rejected", "Text empty, too long, or outbox full.");
    reply = "{\"type\":\"queued\"}";
    return true;
  }
  if (cmd == "sim") {
    // Injected locally; this does not exercise the radio or router.
    static const uint8_t simId[8] = {'M', 'L', 0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    ml::Packet p;
    if (!encodeText(rest.c_str(), p.payload)) return err("rejected", "Text empty or too long.");
    p.mode = ml::AddressMode::Broadcast;  // broadcast: no ack is queued for a simulated sender
    memcpy(p.source, simId, 8);
    p.messageId[7] = 1;
    onPacket(p, -90, nowMs);
    reply = "{\"type\":\"sim\",\"stored\":true}";
    return true;
  }
  return false;
}
}  // namespace mos
