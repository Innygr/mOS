# Mission Link transport boundary (draft)

## Goal

Keep Mission Link's packet format independent of the radio chip and the mOS user interface. The packet codec turns a packet structure into bytes; a transport adapter is responsible for moving those bytes over a configured link.

## Proposed layers

1. **Application/message service** — contact-book lookups, outgoing messages, delivery state and notifications.
2. **Routing layer** — destination selection, hop/TTL handling, duplicate suppression and store-and-forward policy.
3. **Packet codec** — deterministic frame encoding, decoding and CRC validation.
4. **Radio transport adapter** — radio initialization, transmit/receive, channel configuration and radio error reporting.
5. **Hardware configuration** — board-specific SPI, reset, busy, interrupt and antenna-related configuration.

The first radio adapter should be isolated behind a small interface so the codec and message service can be tested on a computer without radio hardware. Do not hardcode GPIO numbers until the exact XIAO + LoRa module/board revision and wiring are confirmed.

## First transport API proposal

The eventual adapter should expose operations equivalent to:

- initialize using an explicit board/radio configuration;
- submit one already-encoded frame for transmission;
- report transmit completion or a bounded error;
- deliver received byte frames to the codec;
- expose radio metrics (RSSI/SNR when available) as metadata, not as trusted identity;
- stop/deinitialize cleanly.

Exact names and ownership rules should be settled when the selected radio driver is known. The adapter must not silently truncate a frame; it must return a clear size/configuration error.

## Reliability and safety requirements

- Validate the frame length before decoding and before handing data to routing.
- Never treat a node ID, sender address, RSSI or CRC as authentication.
- A CRC detects accidental corruption; it does not provide encryption or message authenticity.
- Define duplicate suppression and retry/ACK behavior before claiming reliable delivery.
- Bound queues and message lifetimes to avoid memory exhaustion on a small device.
- Respect the selected radio region's permitted frequency, power and airtime rules.
- Test with a host-side fake transport before connecting real hardware.

## Compatibility status

This is an architecture proposal, not a frozen protocol or a claim of compatibility with Meshtastic, MeshCore or other networks. The Mission Link wire format remains a draft until reviewed and versioned.
