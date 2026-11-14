// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_AVRCP_CONNECTION_INTERNAL_H
#define AIRPODS_AVRCP_CONNECTION_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>
#include "btstack.h"

#define AVRCP_RETRY_MS 1000u
#define AVRCP_RETRY_ATTEMPTS 3u

// One bounded AVRCP-only reconnect for the current A2DP media session.
typedef struct {
    bd_addr_t peer;
    uint16_t media_cid;
    uint32_t since;
    uint8_t attempts;
    bool pending;
} avrcp_reconnect_attempt_t;

#ifdef HOST_TEST_VISIBILITY
// Host tests observe the reconnect state and call the handlers directly.
extern avrcp_reconnect_attempt_t avrcp_retry;
extern uint16_t avrcp_cid;
void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
#endif

#endif
