#ifndef MISSION_LINK_RX_H
#define MISSION_LINK_RX_H

#include "mission_link_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Receive-side validation, destination filtering and duplicate suppression
 * (draft). It does not forward, acknowledge, authenticate, decrypt or persist.
 */

/* Number of recent (source, message ID) pairs remembered. Oldest is evicted first. */
#define ML_DEDUP_CAPACITY 32u

typedef struct {
    uint32_t source[ML_DEDUP_CAPACITY];
    uint32_t message_id[ML_DEDUP_CAPACITY]; /* 0 marks an empty slot; real IDs are nonzero */
    uint8_t next;                           /* next slot to overwrite */
} ml_dedup_t;

typedef enum {
    ML_DEDUP_NEW = 0,
    ML_DEDUP_DUPLICATE,
    ML_DEDUP_INVALID /* message ID 0 or NULL cache */
} ml_dedup_result_t;

void ml_dedup_init(ml_dedup_t *cache);

/* Record the pair if unseen. Returns ML_DEDUP_DUPLICATE if it is still in the cache. */
ml_dedup_result_t ml_dedup_check(ml_dedup_t *cache,
                                 uint32_t source,
                                 uint32_t message_id);

typedef enum {
    ML_RX_IDLE = 0,       /* nothing pending on the transport */
    ML_RX_PACKET,         /* *out holds an accepted packet */
    ML_RX_DROP_INVALID,   /* failed decode/CRC, or a DATA packet with message ID 0 */
    ML_RX_DROP_OWN,       /* source is this node (echo of our own transmission) */
    ML_RX_DROP_NOT_FOR_US,/* destination is neither this node nor broadcast */
    ML_RX_DROP_DUPLICATE  /* DATA packet already seen recently */
} ml_rx_result_t;

typedef struct {
    ml_transport_t *transport;
    uint32_t local_node_id;
    ml_dedup_t dedup;
} ml_rx_t;

bool ml_rx_init(ml_rx_t *rx, ml_transport_t *transport, uint32_t local_node_id);

/*
 * Process at most one pending frame. Call repeatedly until ML_RX_IDLE to drain.
 * On ML_RX_PACKET, *out (and *meta if non-NULL) are filled; otherwise they are
 * left unchanged. Only DATA packets are de-duplicated; the semantics of
 * HELLO/ACK/ROUTE/CONTROL are not yet defined, so they pass through.
 */
ml_rx_result_t ml_rx_poll(ml_rx_t *rx, ml_packet_t *out, ml_rx_meta_t *meta);

#ifdef __cplusplus
}
#endif

#endif
