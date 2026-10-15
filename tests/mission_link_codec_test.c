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

/* Golden frame computed independently (Python binascii.crc_hqx, init 0xFFFF)
 * from the layout in docs/mission-link-protocol.md. Locks the wire format so a
 * symmetric encode/decode mistake cannot pass the round-trip tests. */
static const uint8_t GOLDEN_FRAME[] = {
    0x4D, 0x4C,                         /* magic "ML" */
    0x01,                               /* version */
    0x02,                               /* type DATA */
    0x00,                               /* flags */
    0x04,                               /* ttl */
    0x01, 0x02, 0x03, 0x04,             /* source */
    0xFF, 0xFF, 0xFF, 0xFF,             /* destination: broadcast */
    0xA1, 0xB2, 0xC3, 0xD4,             /* message id */
    0x00, 0x05,                         /* payload length */
    0x68, 0x65, 0x6C, 0x6C, 0x6F,       /* "hello" */
    0x4B, 0x46                          /* CRC-16/CCITT-FALSE */
};

static void test_golden_frame(void)
{
    ml_packet_t packet = {0};
    packet.type = ML_PACKET_DATA;
    packet.ttl = 4;
    packet.source = 0x01020304u;
    packet.destination = ML_BROADCAST_ADDRESS;
    packet.message_id = 0xA1B2C3D4u;
    packet.payload_length = 5;
    memcpy(packet.payload, "hello", 5);

    uint8_t frame[ML_MAX_FRAME];
    const size_t length = ml_packet_encode(&packet, frame, sizeof(frame));
    assert(length == sizeof(GOLDEN_FRAME));
    assert(memcmp(frame, GOLDEN_FRAME, sizeof(GOLDEN_FRAME)) == 0);

    ml_packet_t decoded = {0};
    assert(ml_packet_decode(GOLDEN_FRAME, sizeof(GOLDEN_FRAME), &decoded));
    assert(decoded.source == 0x01020304u);
    assert(decoded.destination == ML_BROADCAST_ADDRESS);
    assert(decoded.message_id == 0xA1B2C3D4u);
    assert(decoded.payload_length == 5u);
    assert(memcmp(decoded.payload, "hello", 5) == 0);
}

/* Re-seal a hand-modified frame with a valid CRC so only the field under test is wrong. */
static void reseal(uint8_t *frame, size_t length)
{
    const uint16_t crc = ml_crc16_ccitt(frame, length - ML_CRC_SIZE);
    frame[length - 2] = (uint8_t)(crc >> 8);
    frame[length - 1] = (uint8_t)crc;
}

static void test_length_field_mismatch_rejected(void)
{
    uint8_t frame[sizeof(GOLDEN_FRAME)];
    ml_packet_t decoded = {0};

    /* Declared payload shorter than the bytes present. */
    memcpy(frame, GOLDEN_FRAME, sizeof(frame));
    frame[19] = 4;
    reseal(frame, sizeof(frame));
    assert(!ml_packet_decode(frame, sizeof(frame), &decoded));

    /* Declared payload longer than the bytes present. */
    memcpy(frame, GOLDEN_FRAME, sizeof(frame));
    frame[19] = 6;
    reseal(frame, sizeof(frame));
    assert(!ml_packet_decode(frame, sizeof(frame), &decoded));

    /* Declared payload above the software ceiling, with a valid CRC. */
    memcpy(frame, GOLDEN_FRAME, sizeof(frame));
    frame[18] = 0x01;
    frame[19] = 0x00;
    reseal(frame, sizeof(frame));
    assert(!ml_packet_decode(frame, sizeof(frame), &decoded));
}

static void test_failed_decode_leaves_output_untouched(void)
{
    ml_packet_t out = {0};
    out.source = 0xDEADBEEFu;
    out.payload_length = 3;

    uint8_t frame[sizeof(GOLDEN_FRAME)];
    memcpy(frame, GOLDEN_FRAME, sizeof(frame));
    frame[sizeof(frame) - 1] ^= 0x01u; /* bad CRC */

    assert(!ml_packet_decode(frame, sizeof(frame), &out));
    assert(out.source == 0xDEADBEEFu);
    assert(out.payload_length == 3u);
}

static void test_oversized_and_short_frames_rejected(void)
{
    uint8_t big[ML_MAX_FRAME + 1u] = {0};
    ml_packet_t decoded = {0};

    assert(!ml_packet_decode(big, sizeof(big), &decoded));
    assert(!ml_packet_decode(GOLDEN_FRAME, 0u, &decoded));
    assert(!ml_packet_decode(GOLDEN_FRAME, ML_HEADER_SIZE + ML_CRC_SIZE - 1u, &decoded));
}

int main(void)
{
    test_golden_frame();
    test_length_field_mismatch_rejected();
    test_failed_decode_leaves_output_untouched();
    test_oversized_and_short_frames_rejected();
    test_crc_vector();
    test_round_trip();
    test_empty_and_maximum_payloads();
    test_reject_bad_frames();
    test_invalid_arguments_and_capacity();
    puts("Mission Link codec tests passed");
    return 0;
}
