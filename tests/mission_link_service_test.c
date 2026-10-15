#include "mission_link_service.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_build_and_read_text(void)
{
    ml_service_t service;
    ml_packet_t packet = {0};
    char text[ML_MAX_PAYLOAD + 1u];

    assert(ml_service_init(&service, 0x10203040u, 7u));
    assert(ml_service_make_text(&service, 0xAABBCCDDu, 3u, "hello", &packet));
    assert(packet.type == ML_PACKET_DATA);
    assert(packet.source == 0x10203040u);
    assert(packet.destination == 0xAABBCCDDu);
    assert(packet.ttl == 3u);
    assert(packet.message_id == 7u);
    assert(packet.payload_length == 5u);
    assert(ml_service_read_text(&packet, text, sizeof(text)));
    assert(strcmp(text, "hello") == 0);

    assert(ml_service_make_text(&service, ML_BROADCAST_ADDRESS, 1u, "", &packet));
    assert(packet.message_id == 8u);
    assert(packet.payload_length == 0u);
    assert(ml_service_read_text(&packet, text, sizeof(text)));
    assert(strcmp(text, "") == 0);
}

static void test_message_id_wrap(void)
{
    ml_service_t service;
    ml_packet_t packet = {0};

    assert(ml_service_init(&service, 42u, UINT32_MAX));
    assert(ml_service_make_text(&service, 9u, 1u, "a", &packet));
    assert(packet.message_id == UINT32_MAX);
    assert(ml_service_make_text(&service, 9u, 1u, "b", &packet));
    assert(packet.message_id == 1u);
}

static void test_reject_invalid_inputs(void)
{
    ml_service_t service;
    ml_packet_t packet = {0};
    char output[5];
    char too_long[ML_MAX_PAYLOAD + 2u];

    memset(too_long, 'x', sizeof(too_long) - 1u);
    too_long[sizeof(too_long) - 1u] = '\0';

    assert(!ml_service_init(NULL, 1u, 1u));
    assert(ml_service_init(&service, 1u, 0u));
    assert(service.next_message_id == 1u);
    assert(!ml_service_make_text(NULL, 2u, 1u, "x", &packet));
    assert(!ml_service_make_text(&service, 2u, 1u, NULL, &packet));
    assert(!ml_service_make_text(&service, 2u, 1u, "x", NULL));
    assert(!ml_service_make_text(&service, 2u, 1u, too_long, &packet));

    packet.type = ML_PACKET_HELLO;
    assert(!ml_service_read_text(&packet, output, sizeof(output)));
    packet.type = ML_PACKET_DATA;
    packet.payload_length = 5u;
    memcpy(packet.payload, "hello", 5u);
    assert(!ml_service_read_text(&packet, output, sizeof(output)));
    assert(!ml_service_read_text(&packet, output, 5u));

    packet.payload_length = 2u;
    packet.payload[0] = 'a';
    packet.payload[1] = 0u;
    assert(!ml_service_read_text(&packet, output, sizeof(output)));
}

int main(void)
{
    test_build_and_read_text();
    test_message_id_wrap();
    test_reject_invalid_inputs();
    puts("Mission Link service tests passed");
    return 0;
}
