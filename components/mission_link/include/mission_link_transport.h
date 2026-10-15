#ifndef MISSION_LINK_TRANSPORT_H
#define MISSION_LINK_TRANSPORT_H

#include "mission_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Radio-independent transport boundary (draft).
 *
 * A transport moves already-encoded frames. It owns radio configuration,
 * channel access and radio errors; it does not interpret addresses, message IDs
 * or packet types. Real SX1262 adapters and the host-side fake used in tests
 * implement the same ml_transport_ops_t.
 */

typedef enum {
    ML_TX_OK = 0,
    ML_TX_ERR_ARG,       /* NULL/empty argument or transport without ops */
    ML_TX_ERR_ENCODE,    /* packet could not be encoded */
    ML_TX_ERR_TOO_LARGE, /* frame exceeds what this transport can carry */
    ML_TX_ERR_BUSY,      /* transport cannot accept a frame right now; retry later */
    ML_TX_ERR_RADIO      /* radio reported a failure */
} ml_tx_status_t;

/* Link metrics are advisory metadata. They are never proof of identity. */
typedef struct {
    bool valid;
    int16_t rssi_dbm;
    int8_t snr_db;
} ml_rx_meta_t;

typedef struct ml_transport ml_transport_t;

typedef struct {
    /* Queue one encoded frame. Must not truncate; return ML_TX_ERR_TOO_LARGE instead. */
    ml_tx_status_t (*send)(ml_transport_t *transport,
                           const uint8_t *frame,
                           size_t length);

    /*
     * Non-blocking receive. Return 0 only when nothing is pending. Otherwise
     * consume one pending frame and return its true length. If that length is
     * <= capacity the frame was copied into buffer; if it is > capacity the
     * frame was discarded (never truncated) and buffer contents are unspecified.
     * Reporting oversize frames this way keeps "0" meaning "queue empty", so a
     * drain loop cannot stop early. meta may be NULL.
     */
    size_t (*receive)(ml_transport_t *transport,
                      uint8_t *buffer,
                      size_t capacity,
                      ml_rx_meta_t *meta);

    /* Largest frame, in bytes, this transport can send in one transmission. */
    size_t (*max_frame_length)(const ml_transport_t *transport);
} ml_transport_ops_t;

struct ml_transport {
    const ml_transport_ops_t *ops;
    void *ctx; /* adapter-private state */
};

/* Validate and send an already-encoded frame (length checked against the transport limit). */
ml_tx_status_t ml_transport_send_frame(ml_transport_t *transport,
                                       const uint8_t *frame,
                                       size_t length);

/* Encode a packet and send it. Returns ML_TX_ERR_ENCODE for packets the codec rejects. */
ml_tx_status_t ml_transport_send_packet(ml_transport_t *transport,
                                        const ml_packet_t *packet);

/*
 * Receive one raw frame. Returns 0 when idle or on invalid arguments, the frame
 * length when it fit, or a length greater than capacity when an oversized frame
 * was discarded (callers must check). meta is zeroed first.
 */
size_t ml_transport_receive(ml_transport_t *transport,
                            uint8_t *buffer,
                            size_t capacity,
                            ml_rx_meta_t *meta);

#ifdef __cplusplus
}
#endif

#endif
