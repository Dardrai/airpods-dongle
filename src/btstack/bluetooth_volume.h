// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_BLUETOOTH_VOLUME_H
#define AIRPODS_BLUETOOTH_VOLUME_H
#include <stdbool.h>
#include <stdint.h>

// AVRCP-facing part of the volume bridge (bluetooth_volume.c). The USB/host
// facing functions stay declared in btstack_avdtp_source.h and phone_sink.h.
// Main/Bluetooth context only.

// Follow the currently selected AirPods; a peer change drops relay state.
void bt_volume_select_peer(void);
// Controller-role volume notifications and SET ABSOLUTE VOLUME responses.
// Returns false when the event is not a volume event for the selected peer.
bool bt_volume_controller_event(const uint8_t *packet, uint16_t size);
// Target-role volume registration from the AirPods.
bool bt_volume_target_event(const uint8_t *packet, uint16_t size);
// True when the volume state is bound to this AirPods address.
bool bluetooth_volume_peer_matches(const uint8_t address[6]);
// An AVRCP channel opened (nonzero CID) or closed (zero) for the selected peer.
void bluetooth_volume_connection_changed(uint16_t avrcp_cid);
// Count AirPods VOLUME UP/DOWN pass-through operations for bt_volume_report().
void bluetooth_volume_record_operation(uint8_t operation_id);

#endif
