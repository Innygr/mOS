#pragma once
// mOS application-layer message payload carried inside a Mission Link Data
// packet. Layout: [kind:1][body]. Kind 1 = UTF-8/ASCII text, kind 2 = one-byte
// canned phrase id (very small on air). This sits above the Mission Link packet
// codec and does NOT provide encryption; see MessagingService::Config.
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace mos {
enum class MsgKind : uint8_t { Text = 1, Canned = 2 };

static constexpr size_t MAX_TEXT = 120;  // keeps frames short; airtime is scarce
static constexpr uint8_t CANNED_COUNT = 6;

// Phrase ids are 1..CANNED_COUNT and are part of the wire format: append only.
const char* cannedPhrase(uint8_t id);

bool encodeText(const char* text, std::vector<uint8_t>& out);
bool encodeCanned(uint8_t id, std::vector<uint8_t>& out);

// Decodes into a NUL-terminated, display-safe string (printable ASCII only;
// anything else becomes '?'). Canned ids resolve to their phrase.
bool decodeMessage(const std::vector<uint8_t>& payload, char* out, size_t cap);
}  // namespace mos
