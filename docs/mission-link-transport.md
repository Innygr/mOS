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

## Transport API (implemented as a draft)

`components/mission_link/include/mission_link_transport.h` defines the interface (`ml_transport_ops_t`: `send`, `receive`, `max_frame_length`) and helpers `ml_transport_send_packet()` / `ml_transport_send_frame()` / `ml_transport_receive()`. `mission_link_rx.h` adds `ml_rx_poll()`, which decodes a received frame, drops invalid frames, our own echoes and frames addressed to other nodes, and suppresses duplicate DATA packets. A host-side fake radio (`tests/fake_transport.c`) implements the same interface with injectable loss, corruption and size limits, so none of this needs hardware.

The real SX1262 adapter is still not written. Contract points an adapter must honour:

- `send` must not truncate; `ml_transport_send_frame()` rejects frames above `max_frame_length` (an SX126x carries at most 255 bytes per LoRa packet).
- `receive` returns 0 only when nothing is pending. A pending frame longer than the caller's buffer is consumed and its true length returned, so callers can tell "discarded oversize frame" from "idle" and drain loops do not stop early.
- RSSI/SNR are advisory metadata only.

### Original proposal

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


## Current host-testable transport orchestration

The new transport module joins the outbox, frame codec, receive pipeline, and draft ACK helpers without assuming a radio driver:

- The prepare function encodes the first queued outbox packet but does not change its state.
- The TX result function marks a packet SENT only after the radio adapter reports success. A failed transmission report leaves it QUEUED for the caller's retry policy.
- The receive function routes valid ACKs through peer/message correlation and DATA through destination filtering/inbox handling. For accepted or recognized duplicate unicast DATA, it can encode an ACK frame into a caller-provided buffer.
- ACK output length is zero when no ACK is warranted or the output buffer cannot fit the frame.

This is orchestration, not a radio adapter. A future adapter must pass the exact received frame length, report actual transmit completion, handle radio errors and timeouts, and apply retry/backoff. The transport code is still volatile, single-threaded, and unauthenticated.


## Host-testable ACK retry scheduler

The retry module is separate from the radio driver and uses a caller-supplied monotonic millisecond clock. After the transport marks a message SENT, the radio integration calls `ml_retry_note_tx()` to record a successful transmit attempt. Poll `ml_retry_poll()` periodically with a base timeout and maximum attempt count:

- The first ACK deadline is the base timeout; later deadlines double per successful attempt.
- A timed-out SENT message is returned to QUEUED while attempts remain, so the normal transport can send it again.
- Once the maximum number of attempts has timed out, the outbox entry becomes FAILED.
- Failed radio transmissions should not be recorded as successful TX attempts; they remain QUEUED and are handled by the radio adapter's immediate error policy.

Retry metadata is currently volatile and is not persisted across restart. The caller must use a monotonic clock and invoke the scheduler from the same serialized context as outbox state changes. This scheduler does not provide routing, authentication, or guaranteed delivery.
