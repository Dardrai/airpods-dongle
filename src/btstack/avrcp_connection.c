/*
 * AVRCP connection to the selected AirPods: the channel and its bounded
 * reconnect, the AVRCP target (AirPods buttons) and controller (volume and
 * battery notifications) event handlers, and their SDP records.
 * Split out of btstack_avdtp_source.c; main/Bluetooth context only.
 */
/*
 * Copyright (C) 2016 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "btstack.h"
#include "btstack_avdtp_source.h"
#include "btstack_hci.h"
#include "btstack_aacp.h"
#include "phone_sink.h"
#include "phone_call.h"
#include "../tinyusb/uac.h"
#include "../tinyusb/media_control.h"
#include "bluetooth_volume.h"
#include "media_session.h"
#include "host_test_visibility.h"
#include "avrcp_connection.h"
#include "avrcp_connection_internal.h"

typedef struct {
    uint8_t track_id[8];
    uint32_t song_length_ms;
    avrcp_playback_status_t status;
    uint32_t song_position_ms; // 0xFFFFFFFF if not supported
} avrcp_play_status_info_t;


avrcp_track_t tracks[] = {
    {.track_id = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}, .track_nr = 2, .title = "USB Audio",
     .artist = "Pico", .album = "wasdwasd0105", .genre = "vivid", .song_length_ms = 12345},
};

avrcp_play_status_info_t play_info;

static uint8_t sdp_avrcp_target_service_buffer[200];
static uint8_t sdp_avrcp_controller_service_buffer[200];

PRIVATE_UNLESS_HOST_TEST avrcp_reconnect_attempt_t avrcp_retry;
// Owned here: the AVRCP channel to the selected AirPods, zero when closed.
PRIVATE_UNLESS_HOST_TEST uint16_t avrcp_cid;

void avrcp_reconnect_reset(void) {
    memset(&avrcp_retry, 0, sizeof(avrcp_retry));
}

static bool avrcp_reconnect_current(void) {
    const uint8_t *selected = (const uint8_t *)get_device_addr();
    return media_session_accepts_incoming() && get_a2dp_connected_flag() && !media_session_profile_change_pending() &&
        avrcp_retry.media_cid != 0 && avrcp_retry.media_cid == media_session_cid() &&
        selected && !memcmp(selected, avrcp_retry.peer, 6) &&
        !memcmp(media_session_active_device(), avrcp_retry.peer, 6);
}

void avrcp_reconnect_schedule(bool immediate) {
    if (avrcp_retry.media_cid != media_session_cid() ||
        memcmp(avrcp_retry.peer, media_session_active_device(), 6)) {
        avrcp_reconnect_reset();
        avrcp_retry.media_cid = media_session_cid();
        memcpy(avrcp_retry.peer, media_session_active_device(), 6);
    }
    avrcp_retry.pending = false;
    avrcp_retry.since = btstack_run_loop_get_time_ms() - (immediate ? AVRCP_RETRY_MS : 0u);
    if (!avrcp_reconnect_current()) avrcp_reconnect_reset();
}

void avrcp_reconnect_poll(uint32_t now) {
    if (!avrcp_reconnect_current()) {
        avrcp_reconnect_reset();
        return;
    }
    if (avrcp_cid || avrcp_retry.pending ||
        avrcp_retry.attempts >= AVRCP_RETRY_ATTEMPTS ||
        (uint32_t)(now - avrcp_retry.since) < AVRCP_RETRY_MS) return;
    ++avrcp_retry.attempts;
    avrcp_retry.pending = true;
    avrcp_retry.since = now;
    // BTstack can already be establishing the same peer and return no new CID.
    // Keep that setup pending; restarting it cannot safely cancel its SDP query.
    if (avrcp_connect(avrcp_retry.peer, &avrcp_cid) != ERROR_CODE_SUCCESS)
        avrcp_retry.pending = false;
}

PRIVATE_UNLESS_HOST_TEST void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (phone_sink_handle_avrcp(packet_type, channel, packet, size)) return;
    UNUSED(channel);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status = ERROR_CODE_SUCCESS;

    if (packet_type != HCI_EVENT_PACKET || size < 3 || packet[1] + 2u != size) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    
    switch (packet[2]){
        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED:
            if (size != 14) return;
            local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
            status = avrcp_subevent_connection_established_get_status(packet);
            avrcp_subevent_connection_established_get_bd_addr(packet, event_addr);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP: Connection failed, local cid 0x%02x, status 0x%02x\n", local_cid, status);
                if (get_a2dp_connected_flag() && !memcmp(event_addr, media_session_active_device(), 6) &&
                    ((!avrcp_cid && avrcp_retry.pending) ||
                     (local_cid != 0 && local_cid == avrcp_cid))) {
                    avrcp_cid = 0;
                    avrcp_reconnect_schedule(false);
                }
                return;
            }
            const uint8_t *expected = get_a2dp_connected_flag() ? media_session_active_device() : (const uint8_t *)get_device_addr();
            if (!media_session_accepts_incoming() || expected == NULL ||
                memcmp(event_addr, expected, sizeof(bd_addr_t)) != 0) {
                avrcp_disconnect(local_cid);
                return;
            }
            bt_volume_select_peer();
            if (!bluetooth_volume_peer_matches(event_addr)) {
                avrcp_disconnect(local_cid);
                return;
            }
            avrcp_reconnect_reset();
            avrcp_cid = local_cid;
            bluetooth_volume_connection_changed(local_cid);
            phone_sink_volume_invalidated();

            printf("AVRCP: Channel to %s successfully opened, avrcp_cid 0x%02x\n", bd_addr_to_str(event_addr), avrcp_cid);
             
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            avrcp_target_set_now_playing_info(avrcp_cid, NULL, sizeof(tracks)/sizeof(avrcp_track_t));
            
            printf("Enable Volume Change notification\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);
            printf("Enable Battery Status Change notification\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED);
            return;
        
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            if (size != 5 || avrcp_subevent_connection_released_get_avrcp_cid(packet) != avrcp_cid) return;
            printf("AVRCP Target: Disconnected, avrcp_cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
            avrcp_cid = 0;
            bluetooth_volume_connection_changed(0);
            phone_sink_volume_invalidated();
            avrcp_reconnect_schedule(false);
            return;
        default:
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("Responding to event 0x%02x failed, status 0x%02x\n", packet[2], status);
    }
}


PRIVATE_UNLESS_HOST_TEST void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (phone_sink_handle_target(packet_type, channel, packet, size)) return;
    UNUSED(channel);
    uint8_t  status = ERROR_CODE_SUCCESS;

    if (packet_type != HCI_EVENT_PACKET || size < 5) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    if (packet[2] == AVRCP_SUBEVENT_OPERATION) {
        // The pinned BTstack emitter reports its scratch capacity, but delivers the used bytes.
        if (size < 8 || packet[7] > AVRCP_MAX_COMMAND_PARAMETER_LENGTH || size != 8u + packet[7] ||
            (packet[1] + 2u != size && packet[1] != 6u + AVRCP_MAX_COMMAND_PARAMETER_LENGTH)) return;
    } else if (packet[1] + 2u != size) return;
    if (avrcp_cid == 0 || little_endian_read_16(packet, 3) != avrcp_cid) return;

    bool button_pressed;
    char const * button_state;
    avrcp_operation_id_t operation_id;

    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            bt_volume_select_peer();
            (void)bt_volume_target_event(packet,size);
            break;
        case AVRCP_SUBEVENT_PLAY_STATUS_QUERY:
            status = avrcp_target_play_status(avrcp_cid, play_info.song_length_ms, play_info.song_position_ms, play_info.status);            
            break;
        // case AVRCP_SUBEVENT_NOW_PLAYING_INFO_QUERY:
        //     status = avrcp_target_now_playing_info(avrcp_cid);
        //     break;
        case AVRCP_SUBEVENT_OPERATION:
            operation_id = avrcp_subevent_operation_get_operation_id(packet);
            button_pressed = avrcp_subevent_operation_get_button_pressed(packet) > 0;
            button_state = button_pressed ? "PRESS" : "RELEASE";

            printf("AVRCP Target: operation %s (%s)\n", avrcp_operation2str(operation_id), button_state);

            if (!button_pressed){
                break;
            }
            if (!aacp_audio_owned()) break;
            if(operation_id==AVRCP_OPERATION_ID_PLAY || operation_id==AVRCP_OPERATION_ID_PAUSE ||
               operation_id==AVRCP_OPERATION_ID_STOP || operation_id==AVRCP_OPERATION_ID_FORWARD ||
               operation_id==AVRCP_OPERATION_ID_BACKWARD)playback_accessory_cancel_automatic_media();
            bluetooth_volume_record_operation((uint8_t)operation_id);
            if (!get_usb_streaming() && !usb_mic_requested() && phone_call_media_control((uint8_t)operation_id)) break;
            if (phone_sink_owns_controls() && !get_usb_streaming() && !aacp_mic_requested()) {
                phone_sink_media_control((uint8_t)operation_id);
                break;
            }
            switch (operation_id) {
                case AVRCP_OPERATION_ID_PLAY: usb_media_enqueue(USB_MEDIA_PLAY); break;
                case AVRCP_OPERATION_ID_PAUSE: usb_media_enqueue(USB_MEDIA_PAUSE); break;
                case AVRCP_OPERATION_ID_FORWARD: usb_media_enqueue(USB_MEDIA_NEXT); break;
                case AVRCP_OPERATION_ID_BACKWARD: usb_media_enqueue(USB_MEDIA_PREVIOUS); break;
                case AVRCP_OPERATION_ID_STOP: usb_media_enqueue(USB_MEDIA_STOP); break;
                default: break;
            }
            break;
        default:
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("Responding to event 0x%02x failed, status 0x%02x\n", packet[2], status);
    }
}

PRIVATE_UNLESS_HOST_TEST void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (phone_sink_handle_controller(packet_type, channel, packet, size)) return;
    UNUSED(channel);
    
    if (packet_type != HCI_EVENT_PACKET || size < 5 || packet[1] + 2u != size) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    if (avrcp_cid == 0 || little_endian_read_16(packet, 3) != avrcp_cid) return;
    if (!avrcp_cid) return;
    bt_volume_select_peer();
    if (bt_volume_controller_event(packet,size)) return;
    
    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_EVENT_BATT_STATUS_CHANGED:
            if (size < 7) break;
            // see avrcp_battery_status_t
            printf("AVRCP Controller: Notification Battery Status 0x%02x\n", avrcp_subevent_notification_event_batt_status_changed_get_battery_status(packet));
            break;
        case AVRCP_SUBEVENT_NOTIFICATION_STATE:
            if (size < 8) break;
            printf("AVRCP Controller: Notification %s - %s\n", 
                avrcp_event2str(avrcp_subevent_notification_state_get_event_id(packet)), 
                avrcp_subevent_notification_state_get_enabled(packet) != 0 ? "enabled" : "disabled");
            break;
        default:
            break;
    }  
}

uint16_t avrcp_connection_cid(void) {
    return avrcp_cid;
}

void avrcp_connection_init(void) {
    // Initialize AVRCP Service
    avrcp_init();
    avrcp_register_packet_handler(&avrcp_packet_handler);
    // Initialize AVRCP Target
    avrcp_target_init();
    avrcp_target_register_packet_handler(&avrcp_target_packet_handler);

    // Initialize AVRCP Controller
    avrcp_controller_init();
    avrcp_controller_register_packet_handler(&avrcp_controller_packet_handler);
}

void avrcp_connection_register_service_records(void) {
    // Create AVRCP Target service record and register it with SDP. We receive Category 1 commands from the headphone, e.g. play/pause
    memset(sdp_avrcp_target_service_buffer, 0, sizeof(sdp_avrcp_target_service_buffer));
    uint16_t supported_features = AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER |
                                  AVRCP_FEATURE_MASK_CATEGORY_MONITOR_OR_AMPLIFIER;
#ifdef AVRCP_BROWSING_ENABLED
    supported_features |= AVRCP_FEATURE_MASK_BROWSING;
#endif
    // NOTE: handle must be unique — 0x10002 is already used by the A2DP source
    // record above; with a duplicate, sdp_register_service() rejects this record
    // (SDP_HANDLE_ALREADY_REGISTERED) and the AVRCP Target service is never advertised.
    avrcp_target_create_sdp_record(sdp_avrcp_target_service_buffer, 0x10004, supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_target_service_buffer);

    // Create AVRCP Controller service record and register it with SDP. We send Category 2 commands to the headphone, e.g. volume up/down
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));
    uint16_t controller_supported_features = AVRCP_FEATURE_MASK_CATEGORY_MONITOR_OR_AMPLIFIER |
                                             AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER;
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, 0x10003, controller_supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_controller_service_buffer);
}
