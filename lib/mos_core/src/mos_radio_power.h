#pragma once
// LoRa receive duty-cycling model (pure maths, no hardware).
//
// A receiver that sleeps between short listen windows only hears a frame whose
// preamble outlasts the sleep: the sender must transmit at least
//   preambleSymbols >= ceil(sleepMs / symbolMs) + listenSymbols
// so one listen window is guaranteed to land inside it. That makes the sender's
// preamble length part of the network contract (Mission Link currently uses
// RadioLib's default of 8 symbols), so mOS does NOT enable duty cycling yet.
// See docs/RADIO_POWER.md. This is a planning model, not a measurement.
#include <math.h>
#include <stdint.h>

namespace mos {
// Time for one LoRa symbol: 2^SF / BW.
inline float loraSymbolMs(uint8_t spreadingFactor, float bandwidthKhz) {
  if (spreadingFactor < 5 || spreadingFactor > 12 || bandwidthKhz <= 0) return 0;
  return static_cast<float>(1u << spreadingFactor) / bandwidthKhz;
}

struct DutyCyclePlan {
  bool valid = false;
  uint16_t senderPreambleSymbols = 0;  // minimum preamble every sender must use
  float extraAirtimeMs = 0;            // added to each transmission vs an 8-symbol preamble
  float listenFraction = 0;            // share of time the receiver is awake (0..1)
  float worstCaseLatencyMs = 0;        // a frame can wait up to one sleep period
};

// listenSymbols: symbols the receiver must hear to detect a preamble (>= 2 is
// typical; larger is more robust and costs power).
inline DutyCyclePlan planDutyCycle(uint8_t sf, float bwKhz, uint32_t sleepMs,
                                   uint8_t listenSymbols = 4, uint16_t defaultPreamble = 8) {
  DutyCyclePlan p;
  const float sym = loraSymbolMs(sf, bwKhz);
  if (sym <= 0 || listenSymbols < 2 || listenSymbols > 32 || sleepMs == 0) return p;
  const float needed = ceilf(static_cast<float>(sleepMs) / sym) + listenSymbols;
  if (needed > 65535.0f) return p;  // preamble length field is 16-bit
  p.valid = true;
  p.senderPreambleSymbols = static_cast<uint16_t>(needed);
  p.extraAirtimeMs = needed > defaultPreamble ? (needed - defaultPreamble) * sym : 0;
  const float listenMs = listenSymbols * sym;
  p.listenFraction = listenMs / (listenMs + static_cast<float>(sleepMs));
  p.worstCaseLatencyMs = static_cast<float>(sleepMs);
  return p;
}
}  // namespace mos
