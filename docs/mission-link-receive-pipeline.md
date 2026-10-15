# Mission Link receive pipeline

The host-testable receive helper connects the frame codec to the bounded inbox.

## Processing order

1. Reject invalid arguments or a corrupt inbox count.
2. Decode the raw frame; the codec validates magic, protocol version, frame length, and CRC.
3. Ignore valid packet types other than DATA for now.
4. Filter DATA packets to the local node ID or broadcast address.
5. Check the inbox for an existing `(source, message_id)` pair.
6. Report duplicates without inserting them again; otherwise enqueue the message.

## Results and ACK recommendation

`ml_receive_frame()` returns a result code, a decoded packet when available, and an advisory `ack_recommended` flag. Unicast DATA packets that are newly accepted or recognized as duplicates can be acknowledged. Broadcast packets are never recommended for ACK, to avoid ACK storms. Frames for other nodes, invalid frames, unsupported packet types, and full inboxes are not acknowledged by this helper.

**No ACK packet is built or transmitted.** The ACK wire format and semantics remain open, so the helper deliberately does not invent them. An eventual transport should define what confirmation means and how it is validated. For now, accepted means added to volatile RAM, not persisted or guaranteed to survive restart.

## Limits

- The inbox and duplicate detection are volatile and fixed-capacity.
- Duplicate recognition lasts only while the original entry remains in the inbox.
- The helper is not thread-safe and does not authenticate senders, encrypt payloads, route packets, or interact with a radio.
- CRC detects accidental corruption; it does not authenticate a packet.
- The broadcast address is a software protocol convention, not a radio-driver configuration.
