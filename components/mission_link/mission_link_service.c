#include "mission_link_service.h"

#include <string.h>

bool ml_service_init(ml_service_t *service,
                     uint32_t local_node_id,
                     uint32_t first_message_id)
{
    if (service == NULL) {
        return false;
    }

    service->local_node_id = local_node_id;
    service->next_message_id = first_message_id == 0u ? 1u : first_message_id;
    return true;
}

bool ml_service_make_text(ml_service_t *service,
                          uint32_t destination,
                          uint8_t ttl,
                          const char *text,
                          ml_packet_t *out)
{
    if (service == NULL || text == NULL || out == NULL) {
        return false;
    }

    const size_t text_length = strlen(text);
    if (text_length > ML_MAX_PAYLOAD) {
        return false;
    }

    ml_packet_t packet = {0};
    packet.type = ML_PACKET_DATA;
    packet.ttl = ttl;
    packet.source = service->local_node_id;
    packet.destination = destination;
    packet.message_id = service->next_message_id;
    packet.payload_length = (uint16_t)text_length;

    if (text_length != 0u) {
        memcpy(packet.payload, text, text_length);
    }

    /* Reserve zero as an invalid/unassigned message ID. */
    service->next_message_id++;
    if (service->next_message_id == 0u) {
        service->next_message_id = 1u;
    }

    *out = packet;
    return true;
}

bool ml_service_read_text(const ml_packet_t *packet,
                          char *out,
                          size_t out_capacity)
{
    if (packet == NULL || out == NULL ||
        packet->type != ML_PACKET_DATA ||
        packet->payload_length > ML_MAX_PAYLOAD ||
        out_capacity <= (size_t)packet->payload_length) {
        return false;
    }

    for (size_t i = 0; i < packet->payload_length; ++i) {
        if (packet->payload[i] == 0u) {
            return false;
        }
    }

    if (packet->payload_length != 0u) {
        memcpy(out, packet->payload, packet->payload_length);
    }
    out[packet->payload_length] = '\0';
    return true;
}
