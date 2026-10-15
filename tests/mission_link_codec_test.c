#include "mission_link.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_crc_vector(void)
{
    static const uint8_t input[] = "123456789";
    assert(ml_crc16_ccitt(input, sizeof(input) - 1u) == 0x29B1u);
}

static void test_round_trip(void)
{
    ml_packet_t original = {0};
    original.type = ML_PACKET_DATA;
    original.ttl = 4;
    original.source = 0x01020304u;
    original.destination = ML_BROADCAST_ADDRESS;
    original.message_id = 0xA1B2C3D4u;
    original.payload_length = 5;
    memcpy(original.payload, "hello", 5);

    uint8_t frame[ML_MAX_FRAME];
    const size_t length = ml_packet_encode(&original, frame, sizeof(frame));
    assert(length == ML_HEADER_SIZE + 5u + ML_CRC_SIZE);

    ml_packet_t decoded = {0};
    assert(ml_packet_decode(frame, length, &decoded));
    assert(decoded.type == original.type);
    assert(decoded.flags == original.flags);
    assert(decoded.ttl == original.ttl);
    assert(decoded.source == original.source);
    assert(decoded.destination == original.destination);
    assert(decoded.message_id == original.message_id);
    assert(decoded.payload_length == original.payload_length);
    assert(memcmp(decoded.payload, "hello", 5) == 0);
}

static void test_empty_and_maximum_payloads(void)
{
    ml_packet_t packet = {0};
    uint8_t frame[ML_MAX_FRAME];
    ml_packet_t decoded = {0};

    size_t length = ml_packet_encode(&packet, frame, sizeof(frame));
    assert(length == ML_HEADER_SIZE + ML_CRC_SIZE);
    assert(ml_packet_decode(frame, length, &decoded));
    assert(decoded.payload_length == 0u);

    packet.payload_length = ML_MAX_PAYLOAD;
    for (size_t i = 0; i < ML_MAX_PAYLOAD; ++i) {
        packet.payload[i] = (uint8_t)i;
    }
    length = ml_packet_encode(&packet, frame, sizeof(frame));
    assert(length == ML_MAX_FRAME);
    assert(ml_packet_decode(frame, length, &decoded));
    assert(decoded.payload_length == ML_MAX_PAYLOAD);
    assert(memcmp(decoded.payload, packet.payload, ML_MAX_PAYLOAD) == 0);
}

static void test_reject_bad_frames(void)
{
    ml_packet_t packet = {0};
    packet.type = ML_PACKET_DATA;
    packet.payload_length = 1u;
    packet.payload[0] = 0x42u;

    uint8_t frame[ML_MAX_FRAME];
    const size_t length = ml_packet_encode(&packet, frame, sizeof(frame));
    assert(length > 0u);

    ml_packet_t decoded = {0};
    assert(!ml_packet_decode(frame, length - 1u, &decoded));

    frame[0] ^= 0x01u;
    assert(!ml_packet_decode(frame, length, &decoded));

    const size_t rebuilt_length = ml_packet_encode(&packet, frame, sizeof(frame));
    assert(rebuilt_length == length);
    frame[ML_HEADER_SIZE] ^= 0x01u; /* Corrupt payload covered by the CRC. */
    assert(!ml_packet_decode(frame, length, &decoded));

    (void)ml_packet_encode(&packet, frame, sizeof(frame));
    frame[2] = (uint8_t)(ML_PROTOCOL_VERSION + 1u);
    assert(!ml_packet_decode(frame, length, &decoded));
}

static void test_invalid_arguments_and_capacity(void)
{
    ml_packet_t packet = {0};
    uint8_t frame[ML_MAX_FRAME];

    assert(ml_packet_encode(NULL, frame, sizeof(frame)) == 0u);
    assert(ml_packet_encode(&packet, NULL, sizeof(frame)) == 0u);
    assert(ml_packet_encode(&packet, frame, ML_HEADER_SIZE) == 0u);
    assert(!ml_packet_decode(NULL, sizeof(frame), &packet));
    assert(!ml_packet_decode(frame, sizeof(frame), NULL));

    packet.payload_length = ML_MAX_PAYLOAD + 1u;
    assert(ml_packet_encode(&packet, frame, sizeof(frame)) == 0u);
}

int main(void)
{
    test_crc_vector();
    test_round_trip();
    test_empty_and_maximum_payloads();
    test_reject_bad_frames();
    test_invalid_arguments_and_capacity();
    puts("Mission Link codec tests passed");
    return 0;
}
