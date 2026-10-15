#include "mission_link_transport.h"

#include <string.h>

static bool transport_usable(const ml_transport_t *transport)
{
    return transport != NULL && transport->ops != NULL &&
           transport->ops->send != NULL &&
           transport->ops->receive != NULL &&
           transport->ops->max_frame_length != NULL;
}

ml_tx_status_t ml_transport_send_frame(ml_transport_t *transport,
                                       const uint8_t *frame,
                                       size_t length)
{
    if (!transport_usable(transport) || frame == NULL || length == 0u) {
        return ML_TX_ERR_ARG;
    }

    if (length > transport->ops->max_frame_length(transport)) {
        return ML_TX_ERR_TOO_LARGE;
    }

    return transport->ops->send(transport, frame, length);
}

ml_tx_status_t ml_transport_send_packet(ml_transport_t *transport,
                                        const ml_packet_t *packet)
{
    if (!transport_usable(transport) || packet == NULL) {
        return ML_TX_ERR_ARG;
    }

    uint8_t frame[ML_MAX_FRAME];
    const size_t length = ml_packet_encode(packet, frame, sizeof(frame));
    if (length == 0u) {
        return ML_TX_ERR_ENCODE;
    }

    return ml_transport_send_frame(transport, frame, length);
}

size_t ml_transport_receive(ml_transport_t *transport,
                            uint8_t *buffer,
                            size_t capacity,
                            ml_rx_meta_t *meta)
{
    if (meta != NULL) {
        memset(meta, 0, sizeof(*meta));
    }

    if (!transport_usable(transport) || buffer == NULL || capacity == 0u) {
        return 0u;
    }

    /* A length above capacity means "oversized frame discarded"; pass it through. */
    return transport->ops->receive(transport, buffer, capacity, meta);
}
