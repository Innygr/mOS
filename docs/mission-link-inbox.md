# Mission Link receive inbox

The receive inbox is a small host-testable storage layer for incoming text DATA
packets. It is separate from frame decoding and from radio transport.

## Behavior

- Fixed capacity of 8 messages; insertion order is preserved.
- Accepts only DATA packets with a nonzero message ID and payload length no
  greater than `ML_MAX_PAYLOAD`.
- Rejects duplicate `(source, message_id)` pairs already in the inbox. IDs are
  scoped to a sender, so two different senders may use the same ID.
- New messages begin unread. The caller may mark them read or remove them.
- Rejects inserts when full and rejects invalid/corrupt counts.
- Uses volatile RAM only; contents disappear on restart.
- Not thread-safe. Callers must synchronize access if used across tasks.

The inbox expects a decoded/validated `ml_packet_t`. The normal receive path
should first call `ml_packet_decode()` on the raw frame, then apply local
destination filtering, and only then pass a DATA packet into the inbox.
A CRC check detects accidental corruption; it does not authenticate the sender.

Duplicate suppression lasts only while the original message remains in the
inbox. Once removed or after restart, the same packet may be accepted again.
A future transport layer should add a bounded duplicate cache independent of
inbox deletion, and define ACK timing so duplicate packets can be acknowledged
without displaying duplicate messages.

## Scope limits

This module does not send ACKs, validate the destination against a local node
ID, persist messages, decrypt/authenticate packets, or interact with the radio.
It is a bounded inbox primitive, not a complete receive pipeline.
