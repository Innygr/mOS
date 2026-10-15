#pragma once
// Mission Link messaging engine (an mOS app's model layer): inbox, outbox with acknowledgement + retry.
// Pure logic (no Arduino/RadioLib) so it is covered by native tests.
#include <stddef.h>
#include <stdint.h>
#include "mos_app.h"
#include "mos_inbox.h"
#include "missionlink_packet.h"

namespace mos {
enum class OutState : uint8_t { Empty, Queued, AwaitingAck, Acked, Sent, Failed };

struct OutItem {
  ml::Packet packet;
  OutState state = OutState::Empty;
  uint8_t attempts = 0;
  uint32_t nextAttemptMs = 0;
  bool wantAck = false;
};

class MessagingService {
 public:
  struct Config {
    // Payloads are plaintext until an authenticated-encryption layer exists
    // (docs/CIPHER_SUITES.md in Mission-Link). While false, nothing is ever
    // handed to the radio sink, regardless of ML_RF_ENABLED.
    bool encryptionReady = false;
    uint8_t maxAttempts = 3;            // provisional
    uint32_t ackTimeoutMs = 15000;      // provisional; doubles per attempt
    uint32_t errorBackoffMs = 5000;
  };
  static constexpr size_t OUTBOX_CAPACITY = 8;

  MessagingService(const uint8_t localId[8], uint32_t bootNonce, const Config& cfg);
  MessagingService(const uint8_t localId[8], uint32_t bootNonce)
      : MessagingService(localId, bootNonce, Config()) {}

  enum class RxOutcome : uint8_t { Ignored, Stored, AckHandled };
  // Feed packets the router said to deliver locally.
  RxOutcome onReceive(const ml::Packet& p, int16_t rssi, uint32_t nowMs);

  bool sendText(const uint8_t dest[8], const char* text, uint32_t nowMs);
  bool sendCanned(const uint8_t dest[8], uint8_t phraseId, uint32_t nowMs);
  bool broadcastText(const char* text, uint32_t nowMs);

  void tick(uint32_t nowMs, IPacketSink& sink);

  Inbox& inbox() { return inbox_; }
  const Inbox& inbox() const { return inbox_; }
  size_t pending() const;          // Queued + AwaitingAck
  bool txBlocked() const { return txBlocked_; }
  const OutItem& outbox(size_t i) const { return out_[i]; }
  const uint8_t* localId() const { return local_; }

 private:
  uint8_t local_[8];
  uint32_t bootNonce_;
  uint32_t counter_ = 0;
  Config cfg_;
  Inbox inbox_;
  OutItem out_[OUTBOX_CAPACITY];
  bool txBlocked_ = false;

  OutItem* freeSlot();
  bool enqueue(ml::AddressMode mode, const uint8_t dest[8], ml::PacketType type,
               const std::vector<uint8_t>& payload, bool wantAck, uint32_t nowMs);
};
}  // namespace mos
