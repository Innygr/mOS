#ifndef MISSION_LINK_H
#define MISSION_LINK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ML_PROTOCOL_VERSION 1u
#define ML_MAX_PAYLOAD 180u
#define ML_HEADER_SIZE 20u
#define ML_CRC_SIZE 2u
#define ML_MAX_FRAME (ML_HEADER_SIZE + ML_MAX_PAYLOAD + ML_CRC_SIZE)
#define ML_BROADCAST_ADDRESS UINT32_C(0xFFFFFFFF)
typedef enum { ML_PACKET_HELLO=1, ML_PACKET_DATA=2, ML_PACKET_ACK=3, ML_PACKET_ROUTE=4, ML_PACKET_CONTROL=5 } ml_packet_type_t;
typedef struct { uint8_t type, flags, ttl; uint32_t source, destination, message_id; uint16_t payload_length; uint8_t payload[ML_MAX_PAYLOAD]; } ml_packet_t;
size_t ml_packet_encode(const ml_packet_t *packet, uint8_t *out, size_t out_capacity);
bool ml_packet_decode(const uint8_t *frame, size_t frame_length, ml_packet_t *out);
uint16_t ml_crc16_ccitt(const uint8_t *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
