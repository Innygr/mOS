#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "mos_message.h"
#include "missionlink_packet.h"

namespace mos {
struct InboxItem {
  uint8_t from[8];
  ml::AddressMode mode;
  char text[MAX_TEXT + 1];
  uint32_t receivedMs;
  int16_t rssi;
  bool read;
};

// Fixed-size ring of received messages; index 0 is the newest. RAM only.
class Inbox {
 public:
  static constexpr size_t CAPACITY = 16;
  size_t count() const { return count_; }
  void push(const uint8_t from[8], ml::AddressMode mode, const char* text,
            uint32_t nowMs, int16_t rssi) {
    head_ = (head_ + CAPACITY - 1) % CAPACITY;  // newest lands at head_
    InboxItem& it = items_[head_];
    memcpy(it.from, from, 8);
    it.mode = mode;
    strncpy(it.text, text, MAX_TEXT);
    it.text[MAX_TEXT] = '\0';
    it.receivedMs = nowMs;
    it.rssi = rssi;
    it.read = false;
    if (count_ < CAPACITY) ++count_;
  }
  const InboxItem* at(size_t i) const {
    return i < count_ ? &items_[(head_ + i) % CAPACITY] : nullptr;
  }
  void markRead(size_t i) {
    if (i < count_) items_[(head_ + i) % CAPACITY].read = true;
  }
  size_t unread() const {
    size_t n = 0;
    for (size_t i = 0; i < count_; ++i) n += at(i)->read ? 0 : 1;
    return n;
  }

 private:
  InboxItem items_[CAPACITY] = {};
  size_t head_ = 0;
  size_t count_ = 0;
};
}  // namespace mos
