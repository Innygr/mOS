#include "mission_link_rx.h"

#include <string.h>

void ml_dedup_init(ml_dedup_t *cache)
{
    if (cache != NULL) {
        memset(cache, 0, sizeof(*cache));
    }
}

ml_dedup_result_t ml_dedup_check(ml_dedup_t *cache,
                                 uint32_t source,
                                 uint32_t message_id)
{
    if (cache == NULL || message_id == 0u) {
        return ML_DEDUP_INVALID;
    }

    for (size_t i = 0; i < ML_DEDUP_CAPACITY; ++i) {
        if (cache->message_id[i] == message_id && cache->source[i] == source) {
            return ML_DEDUP_DUPLICATE;
        }
    }

    cache->source[cache->next] = source;
    cache->message_id[cache->next] = message_id;
    cache->next = (uint8_t)((cache->next + 1u) % ML_DEDUP_CAPACITY);
    return ML_DEDUP_NEW;
}

bool ml_rx_init(ml_rx_t *rx, ml_transport_t *transport, uint32_t local_node_id)
{
    if (rx == NULL || transport == NULL) {
        return false;
    }

    rx->transport = transport;
    rx->local_node_id = local_node_id;
    ml_dedup_init(&rx->dedup);
    return true;
}

ml_rx_result_t ml_rx_poll(ml_rx_t *rx, ml_packet_t *out, ml_rx_meta_t *meta)
{
    if (rx == NULL || rx->transport == NULL || out == NULL) {
        return ML_RX_IDLE;
    }

    uint8_t frame[ML_MAX_FRAME];
    ml_rx_meta_t frame_meta;
    const size_t length = ml_transport_receive(rx->transport, frame, sizeof(frame), &frame_meta);
    if (length == 0u) {
        return ML_RX_IDLE;
    }

    /* Transport discarded a frame longer than any valid Mission Link frame. */
    if (length > sizeof(frame)) {
        return ML_RX_DROP_INVALID;
    }

    ml_packet_t packet;
    if (!ml_packet_decode(frame, length, &packet)) {
        return ML_RX_DROP_INVALID;
    }

    if (packet.source == rx->local_node_id) {
        return ML_RX_DROP_OWN;
    }

    if (packet.destination != rx->local_node_id &&
        packet.destination != ML_BROADCAST_ADDRESS) {
        return ML_RX_DROP_NOT_FOR_US;
    }

    if (packet.type == ML_PACKET_DATA) {
        switch (ml_dedup_check(&rx->dedup, packet.source, packet.message_id)) {
        case ML_DEDUP_NEW:
            break;
        case ML_DEDUP_DUPLICATE:
            return ML_RX_DROP_DUPLICATE;
        default:
            return ML_RX_DROP_INVALID;
        }
    }

    *out = packet;
    if (meta != NULL) {
        *meta = frame_meta;
    }
    return ML_RX_PACKET;
}
