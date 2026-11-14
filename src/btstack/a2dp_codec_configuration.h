// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_A2DP_CODEC_CONFIGURATION_H
#define AIRPODS_A2DP_CODEC_CONFIGURATION_H
#include <stdbool.h>
#include <stdint.h>
#include "playback_options.h"

// Local stream endpoints, the remote endpoint catalog and codec selection
// (a2dp_codec_configuration.c). Main/Bluetooth context only.

// A new signaling connection starts an empty catalog.
void a2dp_codec_configuration_reset(void);
// The signaling connection closed: forget the negotiated AAC/AAC-ELD offers.
void a2dp_codec_configuration_released(void);
// SEP discovery, capability and capabilities-done events. Returns false for others.
bool a2dp_codec_configuration_discovery_event(uint8_t *packet, uint16_t size);
// Highest AAC bitrate this dongle offers and accepts.
int a2dp_codec_configuration_maximum_aac_bitrate(void);
// An AAC configuration was offered by this dongle on the current connection.
bool a2dp_codec_configuration_aac_offered(void);

// Choose a codec (honouring the configured codec option) and send its configuration.
int set_next_codec(uint8_t num);
int setup_sbc_configuration(void);

// Incoming/reconnect AAC-ELD configuration checks against the local offer and known peer.
bool playback_eld_peer_configuration(uint8_t local_seid, uint8_t remote_seid,
                                     const uint8_t *config, uint16_t size);
void playback_eld_bind_configuration(uint8_t local_seid, uint8_t remote_seid);
// Game Mode needs negotiated AAC-ELD with delay reporting on an Apple endpoint.
bool playback_game_eligible(void);
// Whether the current, completely discovered peer supports the requested codec options.
bool playback_profile_preflight(playback_options_t next, const uint8_t peer[6]);

// Debug console access to the catalog.
uint16_t a2dp_codec_configuration_remote_endpoint_count(void);
void a2dp_codec_configuration_print_remote_endpoints(void);
void a2dp_codec_configuration_select_remote_endpoint(uint8_t index);
// Reconfigure the selected remote endpoint to 48 kHz; returns the BTstack status.
uint8_t a2dp_codec_configuration_reconfigure_selected_endpoint(void);

#endif
