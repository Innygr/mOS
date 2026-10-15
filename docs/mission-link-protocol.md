# Mission Link protocol — draft v1 framing

**Status: experimental.** This is an initial frame codec for mOS, not a stable public standard. It does not yet implement routing, acknowledgements, encryption, radio scheduling, or field interoperability.

## Compatibility goals
- Mission Link is a custom protocol, not Meshtastic or MeshCore compatible.
- Watches connect to the network through their own SX1262-class radio and stationary mesh nodes.
- Packet framing is transport-independent; radio drivers must not own message/address semantics.
- Keep frames compact because LoRa airtime and battery are constrained.

## Draft frame layout
Multi-byte integers are big-endian. CRC-16/CCITT-FALSE uses initial value `0xFFFF`, polynomial `0x1021`, no reflection, and no final XOR.

| Offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 2 | Magic | ASCII `ML` (0x4D 0x4C) |
| 2 | 1 | Version | 1 |
| 3 | 1 | Type | 1 HELLO, 2 DATA, 3 ACK, 4 ROUTE, 5 CONTROL |
| 4 | 1 | Flags | Reserved; send zero until defined |
| 5 | 1 | TTL | Hop limit; forwarding not implemented |
| 6 | 4 | Source | Draft 32-bit address |
| 10 | 4 | Destination | Draft 32-bit address; `0xFFFFFFFF` is broadcast |
| 14 | 4 | Message ID | Sender-generated ID for deduplication/retries |
| 18 | 2 | Payload length | 0–180 bytes |
| 20 | 0–180 | Payload | Opaque bytes; app encoding undefined |
| end−2 | 2 | CRC | CRC over header and payload, excluding CRC bytes |

Maximum draft frame is 202 bytes. The 180-byte payload limit is a software ceiling, not a guarantee every LoRa configuration can carry it in one transmission. The radio adapter must enforce actual radio limits and airtime policy.

## Codec behaviour
`ml_packet_encode()` returns encoded length or 0 for invalid arguments/capacity. `ml_packet_decode()` rejects invalid magic/version, lengths, oversized frames, and CRC mismatch. The codec does not authenticate senders or validate packet-type semantics. **CRC is not encryption or security.**

## Before a real network launch
1. Review address ranges for watches, stationary nodes, and broadcast.
2. Specify routing, TTL rules, duplicate cache, ACKs, retry/backoff, and store-and-forward.
3. Specify authenticated identity and end-to-end encryption; addresses and CRC are not proof of identity.
4. Define compact app payloads, contact-book mapping, and delivery states.
5. Add fixed test vectors, fuzzing, and cross-device interoperability tests.
6. Confirm Canadian ISED requirements for the chosen module, antenna, and settings.
