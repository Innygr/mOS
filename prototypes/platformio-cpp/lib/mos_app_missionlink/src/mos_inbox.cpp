#include "mos_inbox.h"

namespace mos {
size_t Inbox::serialize(uint8_t* out, size_t cap) const {
  if (!out || cap < MAX_SERIALIZED) return 0;
  size_t n = 0;
  out[n++] = 1;
  out[n++] = static_cast<uint8_t>(count_);
  for (size_t k = count_; k-- > 0;) {  // oldest first, so loading by push() restores order
    const InboxItem& it = *at(k);
    memcpy(out + n, it.from, 8);
    n += 8;
    out[n++] = static_cast<uint8_t>(it.mode);
    out[n++] = it.read ? 1 : 0;
    const size_t len = strlen(it.text);
    out[n++] = static_cast<uint8_t>(len);
    memcpy(out + n, it.text, len);
    n += len;
  }
  return n;
}

bool Inbox::deserialize(const uint8_t* data, size_t len) {
  Inbox fresh;
  if (!data || len < 2 || data[0] != 1 || data[1] > CAPACITY) return false;
  size_t pos = 2;
  for (uint8_t i = 0; i < data[1]; ++i) {
    if (len - pos < 11) return false;
    const uint8_t* from = data + pos;
    const uint8_t mode = data[pos + 8], read = data[pos + 9], tl = data[pos + 10];
    pos += 11;
    if (mode > static_cast<uint8_t>(ml::AddressMode::Broadcast) || read > 1 || tl == 0 ||
        tl > MAX_TEXT || len - pos < tl) return false;
    char text[MAX_TEXT + 1];
    for (uint8_t c = 0; c < tl; ++c) {
      if (data[pos + c] < 0x20 || data[pos + c] >= 0x7f) return false;
      text[c] = static_cast<char>(data[pos + c]);
    }
    text[tl] = '\0';
    pos += tl;
    fresh.push(from, static_cast<ml::AddressMode>(mode), text, 0, 0);
    if (read) fresh.markRead(0);
  }
  if (pos != len) return false;
  const uint32_t rev = revision_;
  *this = fresh;
  revision_ = rev + 1;
  return true;
}
}  // namespace mos
