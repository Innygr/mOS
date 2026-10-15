# Mission Link message service — draft

**Status: experimental foundation.** This layer builds and reads simple text payloads above the draft packet codec. It does not send radio packets, acknowledge delivery, route messages, encrypt content, or persist messages.

## Current behaviour

- Outgoing messages use packet type `DATA`, a configured local 32-bit node address, destination, TTL, nonzero message ID, and text bytes.
- Message IDs increment per service instance and wrap from `UINT32_MAX` to 1. This is not persistent across reboot and does not by itself prevent collisions between different devices or after a reset.
- Payloads are limited to `ML_MAX_PAYLOAD` (currently 180 bytes). This is a software cap, not the usable payload limit of every LoRa configuration.
- Incoming text extraction requires a DATA packet, enough output space for a trailing NUL, and no embedded NUL bytes. It does not validate UTF-8.
- The service is transport-independent and has no radio or ESP-IDF dependencies, so it can be tested on a host computer.

## Identity is still an open design decision

The 32-bit source/destination addresses in the draft frame are node addresses. The planned human-readable Mission Link contact ID format `(777)-XXX-XXXX` is **not** encoded into these fields. Do not treat the display ID as a numeric radio address or assume a permanent mapping yet. A future identity/contact layer must define provisioning, uniqueness, and what happens when a device is replaced.

## Not yet implemented

Delivery states, ACK/retry policy, store-and-forward, persistent message IDs, contact lookup, UTF-8 validation, authentication, and end-to-end encryption remain future work. A successful call to `ml_service_make_text()` only means a packet structure was prepared; it does not mean a message was transmitted or delivered.

## Receive path (see `mission_link_rx.h`)

`ml_rx_poll()` now provides receive-side duplicate suppression: a fixed cache of the last 32 `(source, message ID)` pairs for DATA packets, evicted oldest-first. Known limitations:

- A sender that reboots and restarts its message IDs at 1 will have its first messages dropped as duplicates until the earlier entries are evicted, and an ID evicted from the cache is accepted again if replayed. Persistent or randomised starting IDs, and authenticated replay protection, are still needed.
- Only DATA packets are de-duplicated; HELLO/ACK/ROUTE/CONTROL semantics are undefined.
- Packets for other nodes are dropped, not forwarded, because routing does not exist yet.
