#include "fake_transport.h"

#include <string.h>

static fake_endpoint_t *endpoint_of(ml_transport_t *transport)
{
    return (fake_endpoint_t *)transport->ctx;
}

bool fake_inject(fake_endpoint_t *endpoint, const uint8_t *frame, size_t length)
{
    if (endpoint == NULL || frame == NULL || length == 0u ||
        length > FAKE_FRAME_BYTES || endpoint->count >= FAKE_INBOX_DEPTH) {
        return false;
    }

    const unsigned slot = (endpoint->head + endpoint->count) % FAKE_INBOX_DEPTH;
    memcpy(endpoint->inbox[slot], frame, length);
    endpoint->inbox_length[slot] = length;
    endpoint->count++;
    return true;
}

static ml_tx_status_t fake_send(ml_transport_t *transport, const uint8_t *frame, size_t length)
{
    fake_endpoint_t *self = endpoint_of(transport);
    if (length > self->max_frame) {
        return ML_TX_ERR_TOO_LARGE;
    }

    self->sent_count++;

    if (self->drop_next_tx > 0u) {
        self->drop_next_tx--;
        self->lost_count++;
        return ML_TX_OK; /* the sender cannot tell a frame was lost in the air */
    }

    uint8_t copy[FAKE_FRAME_BYTES];
    memcpy(copy, frame, length);
    if (self->corrupt_next_tx) {
        self->corrupt_next_tx = false;
        copy[length / 2u] ^= 0x01u;
    }

    if (self->peer == NULL || !fake_inject(self->peer, copy, length)) {
        self->lost_count++;
    }
    return ML_TX_OK;
}

static size_t fake_receive(ml_transport_t *transport,
                           uint8_t *buffer,
                           size_t capacity,
                           ml_rx_meta_t *meta)
{
    fake_endpoint_t *self = endpoint_of(transport);
    if (self->count == 0u) {
        return 0u;
    }

    const size_t length = self->inbox_length[self->head];
    const unsigned slot = self->head;
    self->head = (self->head + 1u) % FAKE_INBOX_DEPTH;
    self->count--;

    if (length > capacity) {
        return length; /* contract: discard, never truncate, report the true length */
    }

    memcpy(buffer, self->inbox[slot], length);
    if (meta != NULL) {
        *meta = self->rx_meta;
    }
    return length;
}

static size_t fake_max_frame(const ml_transport_t *transport)
{
    return ((const fake_endpoint_t *)transport->ctx)->max_frame;
}

static const ml_transport_ops_t FAKE_OPS = {
    fake_send,
    fake_receive,
    fake_max_frame,
};

static void endpoint_init(fake_endpoint_t *endpoint)
{
    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->transport.ops = &FAKE_OPS;
    endpoint->transport.ctx = endpoint;
    endpoint->max_frame = FAKE_FRAME_BYTES;
}

void fake_link_init(fake_endpoint_t *a, fake_endpoint_t *b)
{
    endpoint_init(a);
    endpoint_init(b);
    a->peer = b;
    b->peer = a;
}
