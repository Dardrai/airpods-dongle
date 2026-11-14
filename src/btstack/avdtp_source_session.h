// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_AVDTP_SOURCE_SESSION_H
#define AIRPODS_AVDTP_SOURCE_SESSION_H
#include <stdbool.h>
#include <stdint.h>

// Narrow, read-only view of the A2DP source session that btstack_avdtp_source.c
// owns, for the units split out of it. Main/Bluetooth context only.

// AVDTP signaling channel of the current media session, zero when none.
uint16_t avdtp_source_media_cid(void);
// False after the user released or re-paired the AirPods; incoming profiles are refused.
bool avdtp_source_accepts_incoming(void);
// A codec/profile change is disconnecting and reconnecting the media session.
bool avdtp_source_profile_change_pending(void);
// Address of the AirPods the current media session belongs to.
const uint8_t *avdtp_source_active_device(void);

// Manual AirPods media buttons cancel pending automatic pause/play (owned by
// the accessory automation in btstack_avdtp_source.c).
void playback_accessory_cancel_automatic_media(void);

#endif
