#ifndef MISSION_LINK_ACK_H
#define MISSION_LINK_ACK_H
#include "mission_link_queue.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ACK payload is empty; message_id references the DATA message being confirmed. */
bool ml_ack_make_for_data(const ml_packet_t *received_data,
                          uint32_t local_node_id, ml_packet_t *out_ack);
/* Accept only an empty ACK from the intended recipient for a SENT unicast. */
bool ml_ack_accept(const ml_packet_t *ack, uint32_t local_node_id,
                   ml_outbox_t *outbox);
#ifdef __cplusplus
}
#endif
#endif
