#include "mission_link.h"

#include <string.h>

#define MAGIC0 0x4Du
#define MAGIC1 0x4Cu

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

uint16_t ml_crc16_ccitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;

    if (data == NULL && length != 0u) {
        return 0u;
    }

    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8u; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

size_t ml_packet_encode(const ml_packet_t *packet, uint8_t *out, size_t capacity)
{
    if (packet == NULL || out == NULL ||
        packet->payload_length > ML_MAX_PAYLOAD) {
        return 0u;
    }

    const size_t frame_length = ML_HEADER_SIZE +
                                (size_t)packet->payload_length +
                                ML_CRC_SIZE;
    if (capacity < frame_length) {
        return 0u;
    }

    out[0] = MAGIC0;
    out[1] = MAGIC1;
    out[2] = ML_PROTOCOL_VERSION;
    out[3] = packet->type;
    out[4] = packet->flags;
    out[5] = packet->ttl;
    put32(out + 6, packet->source);
    put32(out + 10, packet->destination);
    put32(out + 14, packet->message_id);
    put16(out + 18, packet->payload_length);

    if (packet->payload_length != 0u) {
        memcpy(out + ML_HEADER_SIZE, packet->payload, packet->payload_length);
    }

    put16(out + frame_length - ML_CRC_SIZE,
          ml_crc16_ccitt(out, frame_length - ML_CRC_SIZE));
    return frame_length;
}

bool ml_packet_decode(const uint8_t *frame, size_t frame_length, ml_packet_t *out)
{
    if (frame == NULL || out == NULL ||
        frame_length < ML_HEADER_SIZE + ML_CRC_SIZE ||
        frame_length > ML_MAX_FRAME) {
        return false;
    }

    if (frame[0] != MAGIC0 || frame[1] != MAGIC1 ||
        frame[2] != ML_PROTOCOL_VERSION) {
        return false;
    }

    const uint16_t payload_length = get16(frame + 18);
    if (payload_length > ML_MAX_PAYLOAD ||
        frame_length != ML_HEADER_SIZE + (size_t)payload_length + ML_CRC_SIZE) {
        return false;
    }

    const uint16_t received_crc = get16(frame + frame_length - ML_CRC_SIZE);
    const uint16_t expected_crc = ml_crc16_ccitt(frame, frame_length - ML_CRC_SIZE);
    if (received_crc != expected_crc) {
        return false;
    }

    /* Decode into a temporary so failure never partially overwrites the caller's packet. */
    ml_packet_t decoded = {0};
    decoded.type = frame[3];
    decoded.flags = frame[4];
    decoded.ttl = frame[5];
    decoded.source = get32(frame + 6);
    decoded.destination = get32(frame + 10);
    decoded.message_id = get32(frame + 14);
    decoded.payload_length = payload_length;

    if (payload_length != 0u) {
        memcpy(decoded.payload, frame + ML_HEADER_SIZE, payload_length);
    }

    *out = decoded;
    return true;
}
