#include "mos_message.h"
#include <string.h>

namespace mos {
static const char* const PHRASES[CANNED_COUNT] = {
    "OK", "Yes", "No", "On my way", "Need help", "Call me"};

const char* cannedPhrase(uint8_t id) {
  if (id < 1 || id > CANNED_COUNT) return nullptr;
  return PHRASES[id - 1];
}

bool encodeText(const char* text, std::vector<uint8_t>& out) {
  out.clear();
  if (!text) return false;
  const size_t n = strlen(text);
  if (n == 0 || n > MAX_TEXT) return false;
  out.push_back(static_cast<uint8_t>(MsgKind::Text));
  out.insert(out.end(), text, text + n);
  return true;
}

bool encodeCanned(uint8_t id, std::vector<uint8_t>& out) {
  out.clear();
  if (!cannedPhrase(id)) return false;
  out.push_back(static_cast<uint8_t>(MsgKind::Canned));
  out.push_back(id);
  return true;
}

bool decodeMessage(const std::vector<uint8_t>& payload, char* out, size_t cap) {
  if (!out || cap == 0) return false;
  out[0] = '\0';
  if (payload.empty()) return false;
  if (payload[0] == static_cast<uint8_t>(MsgKind::Canned)) {
    if (payload.size() != 2) return false;
    const char* phrase = cannedPhrase(payload[1]);
    if (!phrase || strlen(phrase) + 1 > cap) return false;
    strcpy(out, phrase);
    return true;
  }
  if (payload[0] == static_cast<uint8_t>(MsgKind::Text)) {
    const size_t n = payload.size() - 1;
    if (n == 0 || n > MAX_TEXT || n + 1 > cap) return false;
    for (size_t i = 0; i < n; ++i) {
      const uint8_t c = payload[i + 1];
      out[i] = (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '?';
    }
    out[n] = '\0';
    return true;
  }
  return false;
}
}  // namespace mos
