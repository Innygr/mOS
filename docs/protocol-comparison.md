# Mission Link wire format: comparison for the maintainer

Two incompatible "v1" frame formats exist. This compares them; it does not change either.

| | mOS `main` (`docs/mission-link-protocol.md`) | Mission-Link firmware repo (`docs/PACKET_FORMAT.md`) |
|---|---|---|
| Header size | 20 B + 2 B CRC-16 = **22 B overhead** | **32 B**, no CRC |
| Magic | `ML` | none |
| Addresses | 32-bit | 8-byte node IDs |
| Message ID | 32-bit | 8 bytes |
| Hop handling | single TTL | hop limit + hop count |
| Address modes | unicast, broadcast sentinel `0xFFFFFFFF` | direct, group, broadcast |
| Types | HELLO, DATA, ACK, ROUTE, CONTROL | Hello, Data, Ack, Status, Join, Leave |
| ACK | empty payload, message ID echoed | not implemented in firmware yet |
| Max payload | 180 | 180 |
| Surrounding code | C codec, inbox/outbox, retry, transport, tests | C++ codec, mesh router (dedup, rate limits, presets), host gateway/contacts tools, web tools |

## Airtime
The 10-byte difference in overhead costs about 45 ms per frame at SF9/125 kHz with CR 4/5 (about 4.5 ms/byte) and about 64 ms with CR 4/7 (about 6.4 ms/byte, the setting in the Mission-Link firmware). Computed from the standard LoRa bit-rate formula, not measured. The SX1262 can also append its own payload CRC on the radio link, so the application CRC matters mainly for non-radio transports (serial, Bluetooth) and for catching corruption between layers.

## Trade-offs
- **Compact format:** less airtime and a smaller ID space that is still ample (32 bits is about 4.3 billion; collisions among a few hundred nodes are very unlikely, but an assignment scheme should still be defined). Missing: group addressing and a separate hop count.
- **Mission-Link format:** more mesh features and more existing code and tooling, but larger overhead on a link where airtime is the scarce resource.
- Neither format has authentication or encryption; both need that design before private messaging.

## Suggested resolution (maintainer's call)
Nothing is deployed, so converge now rather than later. The compact header, plus an address-mode field for group traffic and whichever hop/TTL semantics the mesh router needs, keeps the airtime saving. That means changing the Mission-Link repo's codec, router tests and host tools, so it should be decided and done there. Until then, mOS main keeps its own format and the PlatformIO prototype keeps the other.
