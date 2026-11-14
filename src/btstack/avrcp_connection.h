// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_AVRCP_CONNECTION_H
#define AIRPODS_AVRCP_CONNECTION_H
#include <stdbool.h>
#include <stdint.h>

// AVRCP channel to the selected AirPods (avrcp_connection.c). Main/Bluetooth context only.

// Register the AVRCP, AVRCP target and AVRCP controller packet handlers.
void avrcp_connection_init(void);
// Create and register the AVRCP target and controller SDP records (after sdp_init).
void avrcp_connection_register_service_records(void);
// Current AVRCP channel, zero when none is open.
uint16_t avrcp_connection_cid(void);

// AVRCP-only reconnect for the current A2DP media session.
void avrcp_reconnect_reset(void);
void avrcp_reconnect_schedule(bool immediate);
void avrcp_reconnect_poll(uint32_t now);

#endif
