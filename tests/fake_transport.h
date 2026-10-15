#ifndef FAKE_TRANSPORT_H
#define FAKE_TRANSPORT_H

#include "mission_link_transport.h"

/*
 * Host-side fake radio for tests. Two endpoints are cross-linked so a frame
 * sent on one appears in the other's inbox. Fault injection lets tests model
 * lost, corrupted and oversized frames without hardware.
 */

#define FAKE_INBOX_DEPTH 8u
#define FAKE_FRAME_BYTES 255u /* SX126x maximum LoRa payload */

typedef struct fake_endpoint {
    ml_transport_t transport;
    struct fake_endpoint *peer;

    /* Configuration / fault injection */
    size_t max_frame;        /* largest frame this endpoint will send */
    unsigned drop_next_tx;   /* silently lose this many outgoing frames */
    bool corrupt_next_tx;    /* flip one bit in the next delivered frame */
    ml_rx_meta_t rx_meta;    /* metadata attached to received frames */

    /* Observability */
    unsigned sent_count;     /* frames accepted by send() */
    unsigned lost_count;     /* frames dropped by fault injection or a full peer inbox */

    /* Inbox */
    uint8_t inbox[FAKE_INBOX_DEPTH][FAKE_FRAME_BYTES];
    size_t inbox_length[FAKE_INBOX_DEPTH];
    unsigned head;
    unsigned count;
} fake_endpoint_t;

/* Initialise two endpoints and link them to each other. */
void fake_link_init(fake_endpoint_t *a, fake_endpoint_t *b);

/* Place raw bytes directly into an endpoint's inbox (bypasses send limits). */
bool fake_inject(fake_endpoint_t *endpoint, const uint8_t *frame, size_t length);

#endif
