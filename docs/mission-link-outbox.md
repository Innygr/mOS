# Mission Link outbox and delivery state

The outbox is a small, fixed-capacity in-memory queue above the packet codec and text-message builder. It currently holds up to 8 DATA packets in insertion order.

## States

- **QUEUED** — prepared for the transport to attempt sending.
- **SENT** — the caller reports that the transport accepted/sent it; this is not proof that a recipient received it.
- **DELIVERED** — the caller reports a delivery confirmation (for example, a future validated ACK handler).
- **FAILED** — the caller has stopped attempting delivery.

The outbox itself does not send radio frames, schedule retries, validate ACKs, persist messages, encrypt content, or provide thread safety. A separate host-testable retry module tracks successful TX attempts in volatile memory and schedules exponential ACK timeouts; radio integration must call it explicitly. The caller explicitly drives state changes. A SENT entry can be returned to QUEUED for a retry. Only DELIVERED and FAILED entries can be removed. Entries disappear on reset because this is volatile RAM.

Message IDs must be nonzero. Duplicate source/message-ID pairs are rejected while present in the outbox. The state-change functions locate entries by message ID, so this outbox is intended for one local sender whose message IDs are unique among its pending entries.

This is an early implementation for host testing, not a frozen public API. Capacity and behavior may change as transport/ACK semantics are specified.
