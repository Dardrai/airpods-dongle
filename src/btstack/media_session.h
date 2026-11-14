// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_MEDIA_SESSION_H
#define AIRPODS_MEDIA_SESSION_H
#include <stdbool.h>
#include <stdint.h>

#include "playback_options.h"

// Narrow view of the A2DP source media session that btstack_avdtp_source.c
// owns, for the units split out of it. Main/Bluetooth context only.
struct avdtp_stream_endpoint;

// AVDTP signaling channel of the current media session, zero when none.
uint16_t media_session_cid(void);
// False after the user released or re-paired the AirPods; incoming profiles are refused.
bool media_session_accepts_incoming(void);
// A codec/profile change is disconnecting and reconnecting the media session.
bool media_session_profile_change_pending(void);
// Address of the AirPods the current media session belongs to.
const uint8_t *media_session_active_device(void);
// Configured codec: 0 none, 1 SBC, 2 AAC, 4 AAC-ELD.
uint8_t media_session_codec(void);
// Codec options the next configuration must honour.
playback_options_t media_session_playback_options(void);
// Endpoint discovery finished and a codec configuration was sent.
bool media_session_codec_scan_finished(void);

// The local and remote stream endpoints the codec configuration selected.
struct avdtp_stream_endpoint *media_session_local_endpoint(void);
void media_session_set_local_endpoint(struct avdtp_stream_endpoint *endpoint);
uint8_t media_session_local_seid(void);
void media_session_set_local_seid(uint8_t local_seid);
uint8_t media_session_remote_seid(void);
void media_session_set_remote_seid(uint8_t remote_seid);

// Debug console commands (bluetooth_debug_console.c).
// Open AVDTP signaling to the selected AirPods, storing the channel in the session.
uint8_t media_session_connect_signaling(void);
// The next AVDTP accept answers a console command; it is logged once.
void media_session_mark_local_command(void);
// Send the pending encoded media packet now.
void media_session_send_media_packet(void);

// Manual AirPods media buttons cancel pending automatic pause/play (owned by
// the accessory automation in btstack_avdtp_source.c).
void playback_accessory_cancel_automatic_media(void);

#endif
