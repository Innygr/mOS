#include "mos_messaging.h"
#include "missionlink_config.h"
#include <string.h>

namespace mos {
MessagingService::MessagingService(const uint8_t localId[8], uint32_t bootNonce,
                                   const Config& cfg)
    : bootNonce_(bootNonce), cfg_(cfg) {
  memcpy(local_, localId, 8);
}

size_t MessagingService::pending() const {
  size_t n = 0;
  for (const OutItem& o : out_)
    if (o.state == OutState::Queued || o.state == OutState::AwaitingAck) ++n;
  return n;
}

OutItem* MessagingService::freeSlot() {
  for (OutItem& o : out_) if (o.state == OutState::Empty) return &o;
  // Reclaim a finished entry before refusing new work.
  for (OutItem& o : out_)
    if (o.state == OutState::Acked || o.state == OutState::Sent ||
        o.state == OutState::Failed) return &o;
  return nullptr;
}

bool MessagingService::enqueue(ml::AddressMode mode, const uint8_t dest[8],
                               ml::PacketType type,
                               const std::vector<uint8_t>& payload, bool wantAck,
                               uint32_t nowMs) {
  OutItem* slot = freeSlot();
  if (!slot) return false;
  ml::Packet p;
  p.type = type;
  p.mode = mode;
  memcpy(p.source, local_, 8);
  if (dest) memcpy(p.destination, dest, 8);
  // Message id = per-boot random nonce || counter. The counter starts at 1 so
  // the id is never all-zero (the router rejects zero ids).
  ++counter_;
  for (int i = 0; i < 4; ++i) {
    p.messageId[i] = static_cast<uint8_t>(bootNonce_ >> (24 - 8 * i));
    p.messageId[4 + i] = static_cast<uint8_t>(counter_ >> (24 - 8 * i));
  }
  p.hopLimit = ML_MAX_HOPS;
  p.hopCount = 0;
  p.payload = payload;
  slot->packet = p;
  slot->state = OutState::Queued;
  slot->attempts = 0;
  slot->nextAttemptMs = nowMs;
  slot->wantAck = wantAck;
  return true;
}

bool MessagingService::sendText(const uint8_t dest[8], const char* text, uint32_t nowMs) {
  std::vector<uint8_t> body;
  if (!dest || !encodeText(text, body)) return false;
  return enqueue(ml::AddressMode::Direct, dest, ml::PacketType::Data, body, true, nowMs);
}

bool MessagingService::sendCanned(const uint8_t dest[8], uint8_t id, uint32_t nowMs) {
  std::vector<uint8_t> body;
  if (!dest || !encodeCanned(id, body)) return false;
  return enqueue(ml::AddressMode::Direct, dest, ml::PacketType::Data, body, true, nowMs);
}

bool MessagingService::broadcastText(const char* text, uint32_t nowMs) {
  std::vector<uint8_t> body;
  if (!encodeText(text, body)) return false;
  static const uint8_t none[8] = {0};
  return enqueue(ml::AddressMode::Broadcast, none, ml::PacketType::Data, body, false, nowMs);
}

MessagingService::RxOutcome MessagingService::onReceive(const ml::Packet& p, int16_t rssi,
                                                        uint32_t nowMs) {
  if (p.type == ml::PacketType::Ack) {
    // Acks are only trusted from the node we sent the message to.
    if (p.mode != ml::AddressMode::Direct || memcmp(p.destination, local_, 8) != 0 ||
        p.payload.size() != 8) return RxOutcome::Ignored;
    for (OutItem& o : out_) {
      if (o.state == OutState::AwaitingAck &&
          memcmp(o.packet.messageId, p.payload.data(), 8) == 0 &&
          memcmp(o.packet.destination, p.source, 8) == 0) {
        o.state = OutState::Acked;
        return RxOutcome::AckHandled;
      }
    }
    return RxOutcome::Ignored;
  }
  if (p.type != ml::PacketType::Data) return RxOutcome::Ignored;
  char text[MAX_TEXT + 1];
  if (!decodeMessage(p.payload, text, sizeof(text))) return RxOutcome::Ignored;
  inbox_.push(p.source, p.mode, text, nowMs, rssi);
  if (p.mode == ml::AddressMode::Direct) {
    // Acknowledge direct messages (best effort, single attempt, not itself acked).
    std::vector<uint8_t> id(p.messageId, p.messageId + 8);
    enqueue(ml::AddressMode::Direct, p.source, ml::PacketType::Ack, id, false, nowMs);
  }
  return RxOutcome::Stored;
}

void MessagingService::tick(uint32_t nowMs, IPacketSink& sink) {
  bool blocked = false;
  for (OutItem& o : out_) {
    if (o.state == OutState::AwaitingAck &&
        static_cast<int32_t>(nowMs - o.nextAttemptMs) >= 0) {
      if (o.attempts >= cfg_.maxAttempts) o.state = OutState::Failed;
      else o.state = OutState::Queued;
    }
    if (o.state != OutState::Queued ||
        static_cast<int32_t>(nowMs - o.nextAttemptMs) < 0) continue;
    if (!cfg_.encryptionReady) { blocked = true; continue; }  // never send plaintext
    switch (sink.transmit(o.packet)) {
      case TxResult::Blocked:
        blocked = true;  // not an attempt: nothing went on air
        break;
      case TxResult::Sent:
        ++o.attempts;
        if (o.wantAck) {
          o.state = OutState::AwaitingAck;
          o.nextAttemptMs = nowMs + (cfg_.ackTimeoutMs << (o.attempts - 1));
        } else {
          o.state = OutState::Sent;
        }
        break;
      case TxResult::Error:
        ++o.attempts;
        if (o.attempts >= cfg_.maxAttempts) o.state = OutState::Failed;
        else o.nextAttemptMs = nowMs + cfg_.errorBackoffMs;
        break;
    }
  }
  txBlocked_ = blocked;
}
}  // namespace mos
