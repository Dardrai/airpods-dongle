// SPDX-License-Identifier: GPL-3.0-only
// Weak no-op collaborators for linking src/btstack/avrcp_connection.c on the host.
// A test defines (strongly) every collaborator whose effect it observes.
#include "btstack.h"
#include "../../src/btstack/btstack_hci.h"
#include "../../src/btstack/btstack_aacp.h"
#include "../../src/btstack/btstack_avdtp_source.h"
#include "../../src/btstack/phone_sink.h"
#include "../../src/btstack/phone_call.h"
#include "../../src/btstack/bluetooth_volume.h"
#include "../../src/btstack/avdtp_source_session.h"
#include "../../src/tinyusb/uac.h"
#include "../../src/tinyusb/media_control.h"
#define WEAK __attribute__((weak))

WEAK uint32_t btstack_run_loop_get_time_ms(void) {return 0;}
WEAK const char *bd_addr_to_str(const bd_addr_t address) {(void)address;return "peer";}
WEAK uint8_t avrcp_connect(bd_addr_t address, uint16_t *avrcp_cid) {(void)address;(void)avrcp_cid;return 0;}
WEAK uint8_t avrcp_disconnect(uint16_t avrcp_cid) {(void)avrcp_cid;return 0;}
WEAK void avrcp_init(void) {}
WEAK void avrcp_register_packet_handler(btstack_packet_handler_t handler) {(void)handler;}
WEAK void avrcp_target_init(void) {}
WEAK void avrcp_target_register_packet_handler(btstack_packet_handler_t handler) {(void)handler;}
WEAK void avrcp_controller_init(void) {}
WEAK void avrcp_controller_register_packet_handler(btstack_packet_handler_t handler) {(void)handler;}
WEAK void avrcp_target_create_sdp_record(uint8_t *service, uint32_t handle, uint16_t features,
                                         const char *name, const char *provider) {
    (void)service;(void)handle;(void)features;(void)name;(void)provider;
}
WEAK void avrcp_controller_create_sdp_record(uint8_t *service, uint32_t handle, uint16_t features,
                                             const char *name, const char *provider) {
    (void)service;(void)handle;(void)features;(void)name;(void)provider;
}
WEAK uint8_t sdp_register_service(const uint8_t *record) {(void)record;return 0;}
WEAK uint8_t avrcp_target_support_event(uint16_t avrcp_cid, uint8_t event) {(void)avrcp_cid;(void)event;return 0;}
WEAK uint8_t avrcp_target_set_now_playing_info(uint16_t avrcp_cid, const avrcp_track_t *track, uint16_t total_tracks) {
    (void)avrcp_cid;(void)track;(void)total_tracks;return 0;
}
WEAK uint8_t avrcp_controller_enable_notification(uint16_t avrcp_cid, uint8_t event) {(void)avrcp_cid;(void)event;return 0;}
WEAK uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t length_ms, uint32_t position_ms,
                                      avrcp_playback_status_t status) {
    (void)avrcp_cid;(void)length_ms;(void)position_ms;(void)status;return 0;
}
WEAK const char *avrcp_operation2str(uint8_t operation) {(void)operation;return "test";}
WEAK const char *avrcp_event2str(uint8_t event) {(void)event;return "test";}

WEAK bd_addr_t *get_device_addr(void) {return NULL;}
WEAK bool get_a2dp_connected_flag(void) {return false;}
WEAK bool get_usb_streaming(void) {return false;}
WEAK bool usb_mic_requested(void) {return false;}
WEAK bool aacp_mic_requested(void) {return false;}
WEAK bool aacp_audio_owned(void) {return false;}
WEAK bool usb_media_enqueue(enum usb_media_command command) {(void)command;return true;}
WEAK bool phone_sink_handle_avrcp(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)type;(void)channel;(void)packet;(void)size;return false;
}
WEAK bool phone_sink_handle_target(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)type;(void)channel;(void)packet;(void)size;return false;
}
WEAK bool phone_sink_handle_controller(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)type;(void)channel;(void)packet;(void)size;return false;
}
WEAK void phone_sink_volume_invalidated(void) {}
WEAK bool phone_sink_owns_controls(void) {return false;}
WEAK bool phone_sink_media_control(uint8_t operation) {(void)operation;return true;}
WEAK bool phone_call_media_control(uint8_t operation) {(void)operation;return false;}
WEAK void bt_volume_select_peer(void) {}
WEAK bool bt_volume_controller_event(const uint8_t *packet, uint16_t size) {(void)packet;(void)size;return false;}
WEAK bool bt_volume_target_event(const uint8_t *packet, uint16_t size) {(void)packet;(void)size;return false;}
WEAK bool bluetooth_volume_peer_matches(const uint8_t address[6]) {(void)address;return true;}
WEAK void bluetooth_volume_connection_changed(uint16_t avrcp_cid) {(void)avrcp_cid;}
WEAK void bluetooth_volume_record_operation(uint8_t operation_id) {(void)operation_id;}
WEAK uint16_t avdtp_source_media_cid(void) {return 0;}
WEAK bool avdtp_source_accepts_incoming(void) {return false;}
WEAK bool avdtp_source_profile_change_pending(void) {return false;}
WEAK const uint8_t *avdtp_source_active_device(void) {static const uint8_t none[6];return none;}
WEAK void playback_accessory_cancel_automatic_media(void) {}
