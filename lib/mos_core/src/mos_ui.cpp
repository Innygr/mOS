#include "mos_ui.h"
#include <stdio.h>
#include <string.h>

namespace mos {
std::vector<std::string> wrapText(const char* text, int cols) {
  std::vector<std::string> lines;
  if (!text || cols < 1) return lines;
  std::string cur;
  const char* p = text;
  while (*p) {
    while (*p == ' ') ++p;
    if (!*p) break;
    const char* start = p;
    while (*p && *p != ' ') ++p;
    std::string word(start, p - start);
    while (static_cast<int>(word.size()) > cols) {  // split overlong words
      if (!cur.empty()) { lines.push_back(cur); cur.clear(); }
      lines.push_back(word.substr(0, cols));
      word.erase(0, cols);
    }
    if (cur.empty()) cur = word;
    else if (static_cast<int>(cur.size() + 1 + word.size()) <= cols) cur += " " + word;
    else { lines.push_back(cur); cur = word; }
  }
  if (!cur.empty()) lines.push_back(cur);
  return lines;
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

Ui::Ui(Canvas& canvas, MessagingService& svc, const WatchClock& clock)
    : cv_(canvas), svc_(svc), clock_(clock) {}

void Ui::showToast(const char* text, uint32_t nowMs) {
  strncpy(toast_, text, sizeof(toast_) - 1);
  toast_[sizeof(toast_) - 1] = '\0';
  toastUntilMs_ = nowMs + TOAST_MS;
}

void Ui::notifyIncoming(uint32_t nowMs) {
  lastActivityMs_ = nowMs;
  showToast("New message", nowMs);
}

void Ui::onGesture(const GestureEvent& ev, uint32_t nowMs) {
  if (ev.gesture == Gesture::None) return;
  const bool wasAsleep = asleep(nowMs);
  lastActivityMs_ = nowMs;
  if (wasAsleep) { go(Screen::Face); return; }  // wake only
  switch (screen_) {
    case Screen::Face: handleFace(ev); break;
    case Screen::Inbox: handleInbox(ev); break;
    case Screen::Message: handleMessage(ev, nowMs); break;
    case Screen::Reply: handleReply(ev, nowMs); break;
    case Screen::Status: handleStatus(ev); break;
  }
}

void Ui::handleFace(const GestureEvent& ev) {
  if (ev.gesture == Gesture::SwipeLeft) go(Screen::Inbox);
  else if (ev.gesture == Gesture::SwipeUp) go(Screen::Status);
}

void Ui::handleInbox(const GestureEvent& ev) {
  const size_t n = svc_.inbox().count();
  const size_t rows = static_cast<size_t>(visibleRows());
  switch (ev.gesture) {
    case Gesture::SwipeRight: go(Screen::Face); break;
    case Gesture::SwipeUp:
      if (scroll_ + rows < n) ++scroll_;
      break;
    case Gesture::SwipeDown:
      if (scroll_ > 0) --scroll_;
      break;
    case Gesture::Tap:
      if (ev.y >= headerH()) {
        const size_t idx = scroll_ + static_cast<size_t>((ev.y - headerH()) / rowH());
        if (idx < n) {
          selected_ = idx;
          svc_.inbox().markRead(idx);
          go(Screen::Message);
        }
      }
      break;
    default: break;
  }
}

int Ui::messageTextRows() const {
  // Header on top, a reply bar at the bottom.
  return (cv_.height() - headerH() - rowH()) / FONT_H;
}

std::vector<std::string> Ui::messageLines() const {
  const InboxItem* it = svc_.inbox().at(selected_);
  return it ? wrapText(it->text, cols()) : std::vector<std::string>();
}

void Ui::handleMessage(const GestureEvent& ev, uint32_t nowMs) {
  switch (ev.gesture) {
    case Gesture::SwipeRight: go(Screen::Inbox); break;
    case Gesture::SwipeUp: {
      const size_t total = messageLines().size();
      if (scroll_ + static_cast<size_t>(messageTextRows()) < total) ++scroll_;
      break;
    }
    case Gesture::SwipeDown:
      if (scroll_ > 0) --scroll_;
      break;
    case Gesture::Tap:
      if (ev.y >= cv_.height() - rowH() && svc_.inbox().at(selected_)) go(Screen::Reply);
      break;
    default: break;
  }
  (void)nowMs;
}

void Ui::handleReply(const GestureEvent& ev, uint32_t nowMs) {
  if (ev.gesture == Gesture::SwipeRight) { go(Screen::Message); return; }
  if (ev.gesture == Gesture::SwipeUp) {
    if (scroll_ + static_cast<size_t>(visibleRows()) < CANNED_COUNT) ++scroll_;
    return;
  }
  if (ev.gesture == Gesture::SwipeDown) { if (scroll_ > 0) --scroll_; return; }
  if (ev.gesture != Gesture::Tap || ev.y < headerH()) return;
  const InboxItem* it = svc_.inbox().at(selected_);
  const int row = (ev.y - headerH()) / rowH();
  const uint8_t id = static_cast<uint8_t>(scroll_ + row + 1);
  if (!it || row >= visibleRows() || !cannedPhrase(id)) return;
  showToast(svc_.sendCanned(it->from, id, nowMs) ? "Queued" : "Outbox full", nowMs);
  go(Screen::Message);
}

void Ui::handleStatus(const GestureEvent& ev) {
  if (ev.gesture == Gesture::SwipeRight || ev.gesture == Gesture::SwipeDown) go(Screen::Face);
}

void Ui::header(const char* title) {
  cv_.fillRect(0, 0, cv_.width(), headerH() - 1, true);
  cv_.drawText(2, 1, title, 1, true);
}

void Ui::render(uint32_t nowMs) {
  cv_.clear();
  if (asleep(nowMs)) { cv_.present(); return; }  // firmware also powers the panel down
  switch (screen_) {
    case Screen::Face: renderFace(nowMs); break;
    case Screen::Inbox: renderInbox(); break;
    case Screen::Message: renderMessage(); break;
    case Screen::Reply: renderReply(); break;
    case Screen::Status: renderStatus(); break;
  }
  if (nowMs < toastUntilMs_) {
    const int w = static_cast<int>(strlen(toast_)) * FONT_W + 4;
    const int x = (cv_.width() - w) / 2, y = cv_.height() - FONT_H - 4;
    cv_.fillRect(x, y, w, FONT_H + 3, false);
    cv_.drawRect(x, y, w, FONT_H + 3);
    cv_.drawText(x + 2, y + 2, toast_);
  }
  cv_.present();
}

void Ui::renderFace(uint32_t nowMs) {
  char buf[24];
  uint8_t h, m, s;
  if (clock_.timeOfDay(nowMs, h, m, s)) snprintf(buf, sizeof(buf), "%02u:%02u", h, m);
  else snprintf(buf, sizeof(buf), "--:--");
  const int scale = 3;
  const int tw = static_cast<int>(strlen(buf)) * FONT_W * scale;
  cv_.drawText((cv_.width() - tw) / 2, 8, buf, scale);
  if (!clock_.valid()) {
    const char* hint = "time not set";
    cv_.drawText((cv_.width() - static_cast<int>(strlen(hint)) * FONT_W) / 2, 36, hint);
  }
  const int y = cv_.height() - FONT_H - 1;
  snprintf(buf, sizeof(buf), "MSG %u", static_cast<unsigned>(svc_.inbox().unread()));
  cv_.drawText(2, y, buf);
  const char* radio = dev_.receiving ? "RX" : "NO RADIO";
  cv_.drawText(cv_.width() - static_cast<int>(strlen(radio)) * FONT_W - 2, y, radio);
}

void Ui::renderInbox() {
  char buf[MAX_TEXT + 16];
  snprintf(buf, sizeof(buf), "Inbox (%u)", static_cast<unsigned>(svc_.inbox().count()));
  header(buf);
  if (svc_.inbox().count() == 0) { cv_.drawText(2, headerH() + 8, "No messages"); return; }
  for (int r = 0; r < visibleRows(); ++r) {
    const InboxItem* it = svc_.inbox().at(scroll_ + static_cast<size_t>(r));
    if (!it) break;
    char id[9];
    shortId(it->from, id);
    // "* " marks unread; sender tag then as much of the text as fits.
    snprintf(buf, sizeof(buf), "%c%.4s %s", it->read ? ' ' : '*', id, it->text);
    if (cols() < static_cast<int>(sizeof(buf))) buf[cols()] = '\0';
    cv_.drawText(0, headerH() + r * rowH() + 2, buf);
  }
}

void Ui::renderMessage() {
  const InboxItem* it = svc_.inbox().at(selected_);
  if (!it) { header("Message"); return; }
  char id[9], buf[32];
  shortId(it->from, id);
  snprintf(buf, sizeof(buf), "From %s", id);
  header(buf);
  const std::vector<std::string> lines = messageLines();
  for (int r = 0; r < messageTextRows(); ++r) {
    const size_t li = scroll_ + static_cast<size_t>(r);
    if (li >= lines.size()) break;
    cv_.drawText(0, headerH() + 1 + r * FONT_H, lines[li].c_str());
  }
  const int by = cv_.height() - rowH();
  cv_.drawRect(0, by, cv_.width(), rowH());
  cv_.drawText(2, by + 2, "[ Reply ]");
}

void Ui::renderReply() {
  header("Reply with");
  for (int r = 0; r < visibleRows(); ++r) {
    const char* phrase = cannedPhrase(static_cast<uint8_t>(scroll_ + r + 1));
    if (!phrase) break;
    cv_.drawText(4, headerH() + r * rowH() + 2, phrase);
  }
}

void Ui::renderStatus() {
  header("Status");
  char buf[40];
  int y = headerH() + 1;
  auto line = [&](const char* s) { cv_.drawText(0, y, s); y += FONT_H; };
  snprintf(buf, sizeof(buf), "Radio %s", dev_.radioState); line(buf);
  snprintf(buf, sizeof(buf), "TX %s  %s", dev_.rfEnabled ? "ON" : "OFF", dev_.preset); line(buf);
  if (dev_.receiving) snprintf(buf, sizeof(buf), "RSSI %d SNR %d", dev_.rssi, dev_.snr);
  else snprintf(buf, sizeof(buf), "RSSI --");
  line(buf);
  snprintf(buf, sizeof(buf), "Outbox %u%s", static_cast<unsigned>(svc_.pending()),
           svc_.txBlocked() ? " (held)" : ""); line(buf);
  if (dev_.batteryPercent >= 0) snprintf(buf, sizeof(buf), "Batt %d%%", dev_.batteryPercent);
  else snprintf(buf, sizeof(buf), "Batt --");
  line(buf);
  snprintf(buf, sizeof(buf), "%s", dev_.nodeIdHex); line(buf);
}
}  // namespace mos
