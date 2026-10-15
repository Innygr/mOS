#ifndef MISSION_LINK_RECEIVE_H
#define MISSION_LINK_RECEIVE_H
#include "mission_link_inbox.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    ML_RECEIVE_ACCEPTED = 0, ML_RECEIVE_DUPLICATE, ML_RECEIVE_NOT_FOR_US,
    ML_RECEIVE_UNSUPPORTED_TYPE, ML_RECEIVE_INVALID_FRAME,
    ML_RECEIVE_INBOX_FULL, ML_RECEIVE_INVALID_ARGUMENT
} ml_receive_result_t;
typedef struct {
    ml_receive_result_t result;
    bool ack_recommended;
    ml_packet_t packet;
} ml_receive_report_t;
/* Advisory only: no ACK is built or transmitted. Broadcasts are never ACKed. */
ml_receive_report_t ml_receive_frame(const uint8_t *frame, size_t frame_length,
                                    uint32_t local_node_id, ml_inbox_t *inbox);
#ifdef __cplusplus
}
#endif
#endif
