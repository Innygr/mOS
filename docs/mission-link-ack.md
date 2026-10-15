# Mission Link ACK draft

This draft defines an ACK using the existing v1 frame header; it adds no payload bytes.

- Type: ACK (3)
- Source: node that received the DATA packet
- Destination: original DATA source
- Message ID: exactly the original DATA message ID
- Payload length: zero
- Version/CRC: ordinary v1 framing and CRC

A receiver may create an ACK only for unicast DATA addressed to its local node. Broadcast messages must not be ACKed. An ACK can mark an outbox message delivered only if its frame was decoded successfully, it is addressed to the local node, it has an empty payload, its message ID matches an entry in SENT state, and its source equals that entry's intended destination.

The helper validates correlation and expected peer, but **this is not authentication**. A malicious node can spoof an address and message ID because the protocol has no cryptographic authentication yet. Do not treat this as secure proof of delivery. The helper also does not transmit ACKs, retry messages, persist state, or implement routing. ACK TTL and routed ACK behavior need to be specified before multi-hop transport is enabled.
