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

## Retry budget and TX failures

Retry metadata lives in a separate module (`mission_link_retry`). Every transmission attempt counts toward one budget (`max_attempts`), whether it was handed to the radio successfully or not:

- A successful TX is recorded with `ml_retry_note_tx()`; if no ACK arrives within the exponential timeout, `ml_retry_poll()` requeues the message, or marks it FAILED once the budget is spent.
- A failed TX (radio busy, driver error) must be recorded with `ml_retry_note_tx_failed()`. Without it a message would be re-offered to a broken radio forever. When the budget is spent the message is marked FAILED immediately. This call does not back off; the caller decides when to try again.
- A message left SENT without a `ml_retry_note_tx()` call is adopted by the next `ml_retry_poll()` (counted as one attempt, timer starting at that poll) so it cannot stay SENT indefinitely.
