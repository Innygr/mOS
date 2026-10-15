#include "mission_link_rx.h"
#include "mission_link_service.h"
#include "fake_transport.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NODE_A 0x0000000Au
#define NODE_B 0x0000000Bu
#define NODE_C 0x0000000Cu

static ml_packet_t data_packet(uint32_t source, uint32_t destination, uint32_t id, const char *text)
{
    ml_packet_t packet = {0};
    packet.type = ML_PACKET_DATA;
    packet.ttl = 3;
    packet.source = source;
    packet.destination = destination;
    packet.message_id = id;
    packet.payload_length = (uint16_t)strlen(text);
    memcpy(packet.payload, text, packet.payload_length);
    return packet;
}

typedef struct {
    fake_endpoint_t a, b;
    ml_rx_t rx_b; /* node B receives what node A sends */
} pair_t;

static void pair_init(pair_t *p)
{
    fake_link_init(&p->a, &p->b);
    assert(ml_rx_init(&p->rx_b, &p->b.transport, NODE_B));
}

static void test_dedup_basics(void)
{
    ml_dedup_t cache;
    ml_dedup_init(&cache);

    assert(ml_dedup_check(&cache, 1u, 10u) == ML_DEDUP_NEW);
    assert(ml_dedup_check(&cache, 1u, 10u) == ML_DEDUP_DUPLICATE);
    assert(ml_dedup_check(&cache, 2u, 10u) == ML_DEDUP_NEW);  /* same ID, other sender */
    assert(ml_dedup_check(&cache, 1u, 11u) == ML_DEDUP_NEW);  /* same sender, other ID */
    assert(ml_dedup_check(&cache, 1u, 0u) == ML_DEDUP_INVALID);
    assert(ml_dedup_check(NULL, 1u, 10u) == ML_DEDUP_INVALID);
}

static void test_dedup_eviction_is_fifo(void)
{
    ml_dedup_t cache;
    ml_dedup_init(&cache);

    for (uint32_t id = 1; id <= ML_DEDUP_CAPACITY; ++id) {
        assert(ml_dedup_check(&cache, 7u, id) == ML_DEDUP_NEW);
    }
    /* Cache is full: everything is still remembered. */
    assert(ml_dedup_check(&cache, 7u, 1u) == ML_DEDUP_DUPLICATE);

    /* One more distinct message evicts the oldest (ID 1), and only that one. */
    assert(ml_dedup_check(&cache, 7u, ML_DEDUP_CAPACITY + 1u) == ML_DEDUP_NEW);
    assert(ml_dedup_check(&cache, 7u, 2u) == ML_DEDUP_DUPLICATE);
    assert(ml_dedup_check(&cache, 7u, ML_DEDUP_CAPACITY + 1u) == ML_DEDUP_DUPLICATE);
    /* Known limitation: an evicted ID is accepted again (replay after eviction). */
    assert(ml_dedup_check(&cache, 7u, 1u) == ML_DEDUP_NEW);
}

static void test_send_and_receive(void)
{
    pair_t p;
    pair_init(&p);
    p.b.rx_meta.valid = true;
    p.b.rx_meta.rssi_dbm = -97;
    p.b.rx_meta.snr_db = 6;

    const ml_packet_t sent = data_packet(NODE_A, NODE_B, 100u, "hello");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);

    ml_packet_t got = {0};
    ml_rx_meta_t meta = {0};
    assert(ml_rx_poll(&p.rx_b, &got, &meta) == ML_RX_PACKET);
    assert(got.source == NODE_A && got.destination == NODE_B);
    assert(got.message_id == 100u && got.ttl == 3u);
    assert(got.payload_length == 5u && memcmp(got.payload, "hello", 5) == 0);
    assert(meta.valid && meta.rssi_dbm == -97 && meta.snr_db == 6);

    assert(ml_rx_poll(&p.rx_b, &got, &meta) == ML_RX_IDLE);
}

static void test_broadcast_accepted(void)
{
    pair_t p;
    pair_init(&p);

    const ml_packet_t sent = data_packet(NODE_A, ML_BROADCAST_ADDRESS, 1u, "all");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);

    ml_packet_t got = {0};
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_PACKET);
    assert(got.destination == ML_BROADCAST_ADDRESS);
}

static void test_filtering(void)
{
    pair_t p;
    pair_init(&p);
    ml_packet_t got = {0};
    got.source = 0xDEADBEEFu;

    /* Addressed to someone else. */
    ml_packet_t sent = data_packet(NODE_A, NODE_C, 1u, "x");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_DROP_NOT_FOR_US);

    /* Echo of our own transmission. */
    sent = data_packet(NODE_B, NODE_A, 2u, "x");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_DROP_OWN);

    /* DATA with the reserved message ID 0. */
    sent = data_packet(NODE_A, NODE_B, 0u, "x");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_DROP_INVALID);

    /* Dropped frames never overwrite the caller's packet. */
    assert(got.source == 0xDEADBEEFu);

    /* Traffic for other nodes must not occupy the duplicate cache. */
    sent = data_packet(NODE_A, NODE_C, 50u, "x");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_DROP_NOT_FOR_US);
    sent = data_packet(NODE_A, NODE_B, 50u, "x");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_PACKET);
}

static void test_duplicates(void)
{
    pair_t p;
    pair_init(&p);
    ml_packet_t got = {0};

    const ml_packet_t sent = data_packet(NODE_A, NODE_B, 5u, "retry me");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK); /* retransmission */
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_PACKET);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_DROP_DUPLICATE);

    /* Same ID from a different source is a different message. */
    const ml_packet_t other = data_packet(NODE_C, NODE_B, 5u, "other");
    assert(ml_transport_send_packet(&p.a.transport, &other) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_PACKET);
    assert(got.source == NODE_C);
}

static void test_non_data_not_deduplicated(void)
{
    pair_t p;
    pair_init(&p);
    ml_packet_t got = {0};

    ml_packet_t hello = data_packet(NODE_A, ML_BROADCAST_ADDRESS, 9u, "");
    hello.type = ML_PACKET_HELLO;
    assert(ml_transport_send_packet(&p.a.transport, &hello) == ML_TX_OK);
    assert(ml_transport_send_packet(&p.a.transport, &hello) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_PACKET);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_PACKET);
}

static void test_lost_and_corrupted_frames(void)
{
    pair_t p;
    pair_init(&p);
    ml_packet_t got = {0};
    const ml_packet_t sent = data_packet(NODE_A, NODE_B, 6u, "payload");

    p.a.drop_next_tx = 1u;
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(p.a.lost_count == 1u);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_IDLE);

    p.a.corrupt_next_tx = true;
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_DROP_INVALID);

    /* A clean retransmission still gets through: corrupt frames were not cached. */
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_PACKET);
}

static void test_send_errors(void)
{
    pair_t p;
    pair_init(&p);
    const ml_packet_t sent = data_packet(NODE_A, NODE_B, 7u, "0123456789");

    /* Frame is 32 bytes; transport only allows 31. Nothing may be delivered. */
    p.a.max_frame = ML_HEADER_SIZE + 10u + ML_CRC_SIZE - 1u;
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_ERR_TOO_LARGE);
    assert(p.a.sent_count == 0u);
    assert(p.b.count == 0u);

    /* Packet the codec rejects. */
    ml_packet_t bad = sent;
    bad.payload_length = ML_MAX_PAYLOAD + 1u;
    p.a.max_frame = FAKE_FRAME_BYTES;
    assert(ml_transport_send_packet(&p.a.transport, &bad) == ML_TX_ERR_ENCODE);

    /* Argument checks. */
    const uint8_t byte = 0u;
    assert(ml_transport_send_packet(NULL, &sent) == ML_TX_ERR_ARG);
    assert(ml_transport_send_packet(&p.a.transport, NULL) == ML_TX_ERR_ARG);
    assert(ml_transport_send_frame(&p.a.transport, NULL, 4u) == ML_TX_ERR_ARG);
    assert(ml_transport_send_frame(&p.a.transport, &byte, 0u) == ML_TX_ERR_ARG);

    ml_transport_t no_ops = {0};
    assert(ml_transport_send_packet(&no_ops, &sent) == ML_TX_ERR_ARG);
}

static void test_oversized_received_frame_discarded(void)
{
    pair_t p;
    pair_init(&p);
    ml_packet_t got = {0};

    uint8_t junk[FAKE_FRAME_BYTES];
    memset(junk, 0xAB, sizeof(junk));
    assert(ML_MAX_FRAME < sizeof(junk));
    assert(fake_inject(&p.b, junk, sizeof(junk)));

    /* Longer than any valid frame: consumed and reported as a drop (not idle), so a
     * drain loop keeps going and the following valid frame is still delivered. */
    const ml_packet_t sent = data_packet(NODE_A, NODE_B, 8u, "after");
    assert(ml_transport_send_packet(&p.a.transport, &sent) == ML_TX_OK);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_DROP_INVALID);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_PACKET);
    assert(ml_rx_poll(&p.rx_b, &got, NULL) == ML_RX_IDLE);
}

static void test_rx_argument_checks(void)
{
    pair_t p;
    pair_init(&p);
    ml_packet_t got = {0};

    assert(!ml_rx_init(NULL, &p.b.transport, NODE_B));
    assert(!ml_rx_init(&p.rx_b, NULL, NODE_B));
    assert(ml_rx_poll(NULL, &got, NULL) == ML_RX_IDLE);
    assert(ml_rx_poll(&p.rx_b, NULL, NULL) == ML_RX_IDLE);
}

static void test_end_to_end_with_text_service(void)
{
    pair_t p;
    pair_init(&p);

    ml_service_t service;
    assert(ml_service_init(&service, NODE_A, 1u));

    ml_packet_t outgoing;
    assert(ml_service_make_text(&service, NODE_B, 3u, "Mission to Vancouver", &outgoing));
    assert(ml_transport_send_packet(&p.a.transport, &outgoing) == ML_TX_OK);

    ml_packet_t incoming = {0};
    char text[ML_MAX_PAYLOAD + 1u];
    assert(ml_rx_poll(&p.rx_b, &incoming, NULL) == ML_RX_PACKET);
    assert(ml_service_read_text(&incoming, text, sizeof(text)));
    assert(strcmp(text, "Mission to Vancouver") == 0);
}

int main(void)
{
    test_dedup_basics();
    test_dedup_eviction_is_fifo();
    test_send_and_receive();
    test_broadcast_accepted();
    test_filtering();
    test_duplicates();
    test_non_data_not_deduplicated();
    test_lost_and_corrupted_frames();
    test_send_errors();
    test_oversized_received_frame_discarded();
    test_rx_argument_checks();
    test_end_to_end_with_text_service();
    puts("Mission Link transport/rx tests passed");
    return 0;
}
