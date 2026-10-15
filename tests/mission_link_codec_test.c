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
    original.flags = 0;
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
    assert(decoded.ttl == original.ttl);
    assert(decoded.source == original.source);
    assert(decoded.destination == original.destination);
    assert(decoded.message_id == original.message_id);
    assert(decoded.payload_length == original.payload_length);
    assert(memcmp(decoded.payload, "hello", 5) == 0);
}

static void test_reject_bad_frames(void)
{
    ml_packet_t packet = {0};
    packet.type = ML_PACKET_HELLO;
    uint8_t frame[ML_MAX_FRAME];
    size_t length = ml_packet_encode(&packet, frame, sizeof(frame));
    assert(length > 0);

    ml_packet_t decoded;
    assert(!ml_packet_decode(frame, length - 1u, &decoded));
    frame[0] ^= 0x01u;
    assert(!ml_packet_decode(frame, length, &decoded));

    length = ml_packet_encode(&packet, frame, sizeof(frame));
    frame[ML_HEADER_SIZE] ^= 0x01u; /* corrupt CRC-covered payload area */
    assert(!ml_packet_decode(frame, length, &decoded));
}

static void test_capacity_and_payload_limits(void)
{
    ml_packet_t packet = {0};
    uint8_t frame[ML_MAX_FRAME];
    assert(ml_packet_encode(&packet, frame, ML_HEADER_SIZE) == 0u);
    packet.payload_length = ML_MAX_PAYLOAD + 1u;
    assert(ml_packet_encode(&packet, frame, sizeof(frame)) == 0u);
}

int main(void)
{
    test_crc_vector();
    test_round_trip();
    test_reject_bad_frames();
    test_capacity_and_payload_limits();
    puts("Mission Link codec tests passed");
    return 0;
}
