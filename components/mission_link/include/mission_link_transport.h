#ifndef MISSION_LINK_TRANSPORT_H
#define MISSION_LINK_TRANSPORT_H
#include "mission_link_ack.h"
#include "mission_link_receive.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    ML_TRANSPORT_RX_DATA = 0,
    ML_TRANSPORT_RX_ACK_ACCEPTED,
    ML_TRANSPORT_RX_ACK_REJECTED,
    ML_TRANSPORT_RX_INVALID
} ml_transport_rx_kind_t;
typedef struct {
    ml_transport_rx_kind_t kind;
    ml_receive_report_t data;
    size_t ack_frame_length;
} ml_transport_rx_report_t;

/* Encodes the first QUEUED outbox entry without changing its delivery state. */
size_t ml_transport_prepare_next(const ml_outbox_t *outbox, uint8_t *frame,
                                 size_t frame_capacity, uint32_t *message_id_out);
/* Call after the radio adapter reports TX success; failure leaves it QUEUED. */
bool ml_transport_report_tx(ml_outbox_t *outbox, uint32_t message_id,
                             bool tx_succeeded);
/* Handles decoded ACKs or DATA frames; ACK output buffer may be NULL. */
ml_transport_rx_report_t ml_transport_receive(const uint8_t *frame,
                                              size_t frame_length,
                                              uint32_t local_node_id,
                                              ml_inbox_t *inbox,
                                              ml_outbox_t *outbox,
                                              uint8_t *ack_frame,
                                              size_t ack_capacity);
#ifdef __cplusplus
}
#endif
#endif
