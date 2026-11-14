/*
 * Bluetooth side of the volume bridge: AVRCP absolute volume for the selected
 * AirPods, the explicit host volume authority, and the USB host relay.
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
#include "pico/time.h"
#include "phone_sink.h"
#include "volume_scale.h"
#include "volume_sync.h"
#include "volume_trace.h"
#include "volume_authority.h"
#include "../tinyusb/dongle_control.h"
#include "../tinyusb/uac.h"
#include "../tinyusb/media_control.h"
#include "btstack_hci.h"
#include "btstack_aacp.h"
#include "host_test_visibility.h"
#include "bluetooth_volume.h"
#include "bluetooth_volume_internal.h"

PRIVATE_UNLESS_HOST_TEST volume_sync_t sink_volume;
PRIVATE_UNLESS_HOST_TEST uint32_t volume_operations_up, volume_operations_down;
PRIVATE_UNLESS_HOST_TEST uint32_t volume_changed_reports, volume_interim_reports, volume_ack_reports;
PRIVATE_UNLESS_HOST_TEST uint32_t volume_target_requests;
PRIVATE_UNLESS_HOST_TEST bluetooth_volume_relay_t usb_volume_relay;
PRIVATE_UNLESS_HOST_TEST volume_authority_t usb_host_volume;
PRIVATE_UNLESS_HOST_TEST uint32_t volume_observation_revision;
PRIVATE_UNLESS_HOST_TEST bool host_volume_write;
PRIVATE_UNLESS_HOST_TEST bool host_volume_quiet;
PRIVATE_UNLESS_HOST_TEST bool _bt_sink_volume_changed;

PRIVATE_UNLESS_HOST_TEST volume_authority_scope_t bt_volume_authority_scope(void) {
    volume_authority_scope_t scope={
        .usb_generation=usb_media_generation(),
        .control_generation=dongle_control_client_generation(DONGLE_CLIENT_USB),
        .cid=sink_volume.cid,
        .ready=usb_audio_available() && usb_host_active() && get_a2dp_connected_flag() &&
            aacp_audio_owned() && !phone_sink_owns_controls(),
    };
    const uint8_t *peer=(const uint8_t *)get_device_addr();
    if (peer) memcpy(scope.peer,peer,sizeof(scope.peer));
    return scope;
}
static bool bt_volume_host_managed(void) { return usb_host_volume.active || host_volume_quiet; }
static void bt_volume_observed(void) {
    if (!++volume_observation_revision) ++volume_observation_revision;
}
PRIVATE_UNLESS_HOST_TEST void bt_volume_relay_reset(bool peer_changed) {
    uint32_t generation=usb_media_generation();
    if (peer_changed || usb_volume_relay.generation!=generation || usb_volume_relay.cid!=sink_volume.cid) {
        bool deferred=!peer_changed && usb_volume_relay.generation==generation && usb_volume_relay.host_pending &&
            (aacp_audio_owned() || usb_volume_relay.host_deferred) &&
            usb_volume_relay.set_revision==usb_volume_set_revision() && usb_volume_set_generation()==generation;
        uint8_t host_level=usb_volume_relay.host_level;
        usb_media_cancel_volume();
        // Only a current explicit host request can await a replacement channel.
        if (!sink_volume.phone_request && !deferred)
            sink_volume.pending=false;
        memset(&usb_volume_relay,0,sizeof(usb_volume_relay));
        usb_volume_relay.generation=generation;
        usb_volume_relay.cid=sink_volume.cid;
        usb_volume_relay.set_revision=usb_volume_set_revision();
        if (deferred) {
            usb_volume_relay.host_level=host_level;
            usb_volume_relay.host_pending=usb_volume_relay.host_deferred=true;
            usb_volume_relay.host_exact=true;
        }
    }
}
static void bt_volume_host_end(void) {
    if (host_volume_write && !sink_volume.phone_request) sink_volume.pending=false;
    host_volume_write=false;
    bt_volume_relay_reset(true);
    host_volume_quiet=true; // A lost bridge cannot prove that Android restored software volume.
    _bt_sink_volume_changed=false;
}
static void bt_volume_authority_poll(void) {
    if (!usb_host_volume.active && !host_volume_quiet) return;
    volume_authority_scope_t scope=bt_volume_authority_scope();
    if (volume_authority_poll(&usb_host_volume,&scope,btstack_run_loop_get_time_ms()))
        bt_volume_host_end();
    if (host_volume_quiet && usb_host_volume.scope.usb_generation!=scope.usb_generation)
        host_volume_quiet=false;
}
void dongle_volume_client_disconnect(uint8_t client) {
    if (volume_authority_disconnect(&usb_host_volume,client)) bt_volume_host_end();
}
uint16_t dongle_volume_snapshot(uint8_t client,uint8_t *out,uint16_t capacity) {
    if (!out || capacity<VOLUME_AUTHORITY_SNAPSHOT_SIZE) return 0;
    volume_authority_scope_t scope=bt_volume_authority_scope();
    uint32_t now=btstack_run_loop_get_time_ms();
    bool active=volume_authority_current(&usb_host_volume,&scope,now);
    bool observed=sink_volume.selected && sink_volume.known && !usb_volume_relay.readback_pending;
    memset(out,0,VOLUME_AUTHORITY_SNAPSHOT_SIZE);
    out[0]=1;
    out[1]=(observed ? 1 : 0) |
        (sink_volume.pending ? 2 : 0) | (sink_volume.inflight ? 4 : 0) |
        (active ? 8 : 0) | (volume_authority_scope_ready(&scope) ? 16 : 0);
    out[2]=observed ? sink_volume.confirmed : 255;
    out[3]=sink_volume.pending || sink_volume.inflight ? sink_volume.desired : 255;
    dongle_put32(out+4,volume_observation_revision);
    if (volume_authority_owner(&usb_host_volume,client,&scope,now))
        dongle_put32(out+8,usb_host_volume.token);
    dongle_put32(out+12,volume_authority_remaining(&usb_host_volume,&scope,now));
    dongle_put32(out+16,scope.usb_generation);
    return VOLUME_AUTHORITY_SNAPSHOT_SIZE;
}
uint8_t dongle_volume_request(uint8_t client,const uint8_t *p,uint16_t size,
                              uint8_t *out,uint16_t capacity,uint16_t *length) {
    if (!p || size!=7 || p[0]!=17 || !out || !length || capacity<VOLUME_AUTHORITY_SNAPSHOT_SIZE ||
        p[1]>3 || p[6]>127 || (p[1]!=3 && p[6]) ||
        (!p[1] && dongle_u32(p+2)) || (p[1] && !dongle_u32(p+2))) return DONGLE_MALFORMED;
    if (client!=DONGLE_CLIENT_USB) return DONGLE_UNSUPPORTED;
    bt_volume_authority_poll();
    volume_authority_scope_t scope=bt_volume_authority_scope();
    if (!volume_authority_scope_ready(&scope)) return DONGLE_DISCONNECTED;
    if (!p[1] && (sink_volume.pending || sink_volume.inflight || !sink_volume.known ||
                  usb_volume_relay.readback_pending)) return DONGLE_BUSY;
    uint8_t result=volume_authority_manage(&usb_host_volume,client,&scope,p[1],
        dongle_u32(p+2),p[6],btstack_run_loop_get_time_ms());
    if (result!=DONGLE_ACCEPTED) return result;
    if (!p[1]) {
        bt_volume_relay_reset(true);
        host_volume_quiet=false;
        _bt_sink_volume_changed=false;
    } else if (p[1]==2) bt_volume_host_end();
    else if (p[1]==3) {
        (void)volume_sync_request(&sink_volume,p[6],false,btstack_run_loop_get_time_ms());
        host_volume_write=sink_volume.pending;
    }
    *length=dongle_volume_snapshot(client,out,capacity);
    return DONGLE_ACCEPTED;
}

static volume_trace_t volume_history;
uint32_t bt_volume_trace_control(bool arm) {
    if (arm) {
        volume_trace_arm(&volume_history,time_us_32());
        return 0;
    }
    return volume_trace_freeze(&volume_history);
}

bool bt_volume_trace_take(uint32_t out[4]) {
    return volume_trace_take(&volume_history,out);
}
static void bt_volume_trace(uint8_t kind,uint8_t ctype,uint8_t label,uint8_t level,uint8_t before,uint8_t status) {
    if (!volume_history.armed) return;
    uint32_t flags=(unsigned)usb_volume_relay.host_pending<<3 |
        (unsigned)usb_volume_relay.readback_pending<<4;
    volume_trace_record(&volume_history,time_us_32(),kind | (uint32_t)(ctype&15)<<4 |
        (uint32_t)(label&15)<<8 | flags<<12 | (uint32_t)status<<24,
        level | (uint32_t)before<<8 | (uint32_t)sink_volume.desired<<16 | (uint32_t)sink_volume.sent<<24,
        usb_media_volume_sent()&0xffffu);
}
void bt_volume_select_peer(void) {
    bool changed=volume_sync_select(&sink_volume, (const uint8_t *)get_device_addr());
    if (changed)
        phone_sink_volume_invalidated();
    bt_volume_relay_reset(changed);
}
static bool bt_volume_usb_owned(void) {
    return aacp_audio_owned() && usb_host_active() && !phone_sink_owns_controls();
}
bool bt_volume_usb_ready(void) {
    return !bt_volume_host_managed() && bt_volume_usb_owned() && !usb_volume_relay.readback_pending &&
        usb_volume_relay.set_revision==usb_volume_set_revision();
}
void bt_volume_report(void) {
    printf("[VOLUME] usb=%lu active=%u playback=%u mic=%u set=%lu session=%lu receipt=%lu sent=%lu relay=0 waiting=0 owned=%u anchor=0 restoring=0 readback=%u epoch=%lu\n",
        (unsigned long)usb_media_generation(),(unsigned)usb_host_active(),(unsigned)get_usb_streaming(),
        (unsigned)usb_mic_requested(),(unsigned long)usb_volume_set_revision(),(unsigned long)usb_volume_set_generation(),
        (unsigned long)usb_volume_set_receipt(),(unsigned long)usb_media_volume_sent(),
        (unsigned)bt_volume_usb_owned(),(unsigned)usb_volume_relay.readback_pending,
        (unsigned long)usb_volume_relay.physical_generation);
    printf("[VOLUME] cid=%04x selected=%u known=%u confirmed=%u pending=%u inflight=%u desired=%u attempts=%u target_up=%lu target_down=%lu changed=%lu interim=%lu ack=%lu target_set=%lu target_known=%u target_level=%u\n",
        (unsigned)sink_volume.cid,(unsigned)sink_volume.selected,(unsigned)sink_volume.known,(unsigned)sink_volume.confirmed,
        (unsigned)sink_volume.pending,(unsigned)sink_volume.inflight,(unsigned)sink_volume.desired,(unsigned)sink_volume.attempts,
        (unsigned long)volume_operations_up,(unsigned long)volume_operations_down,(unsigned long)volume_changed_reports,
        (unsigned long)volume_interim_reports,(unsigned long)volume_ack_reports,(unsigned long)volume_target_requests,
        (unsigned)usb_volume_relay.target_known,(unsigned)usb_volume_relay.target_level);
}
static void bt_volume_publish(void) {
    if (!aacp_audio_owned() || bt_volume_host_managed()) return;
    uint8_t level;
    if (volume_sync_get(&sink_volume, &level)) {
        // Keep the host's finer dB value when AVRCP confirms its coarser mapping.
        if (usb_volume_relay.host_exact && usb_volume_relay.generation==usb_media_generation() &&
            usb_volume_relay.set_revision==usb_volume_set_revision() && level==usb_volume_relay.host_level) return;
        usb_volume_relay.host_exact=false;
        _bt_sink_volume_changed = true;
        phone_sink_volume_changed(level);
    } else if (!sink_volume.pending && !sink_volume.inflight) {
        phone_sink_volume_invalidated();
    }
}
void bt_volume_cancel_phone(void) { volume_sync_cancel_phone(&sink_volume); }
bool get_bt_volume(uint8_t *level) {
    bt_volume_select_peer();
    return volume_sync_get(&sink_volume, level);
}
void set_bt_volume(int16_t db256) {
    bt_volume_select_peer();
    if (volume_sync_request(&sink_volume, usb_volume_to_bt(db256), false,
                           btstack_run_loop_get_time_ms()) && !sink_volume.pending)
        bt_volume_publish();
}
void set_bt_volume_from_usb(int16_t db256,uint32_t revision,uint32_t generation,uint32_t receipt) {
    (void)receipt; // Retained in the USB callback contract; volume no longer emits relative HID keys.
    if (revision!=usb_volume_set_revision() || generation!=usb_media_generation() || !usb_host_active()) return;
    bt_volume_select_peer();
    bt_volume_authority_poll();
    if (bt_volume_host_managed()) {
        usb_volume_relay.set_revision=revision;
        return; // The explicit host bridge owns gain; UAC initialization is not a second volume intent.
    }
    bt_volume_trace(VOLUME_TRACE_HOST,0,0,usb_volume_to_bt(db256),sink_volume.confirmed,0);
    usb_volume_relay.set_revision=revision;
    usb_volume_relay.host_pending=true;
    usb_volume_relay.host_exact=true;
    _bt_sink_volume_changed=false;
    usb_volume_relay.readback_pending=false;
    usb_volume_relay.host_level=usb_volume_to_bt(db256);
    usb_volume_relay.host_deferred=!aacp_audio_owned();
    if (!usb_volume_relay.host_deferred) set_bt_volume(db256);
}
static void bt_volume_readback(uint32_t after,uint32_t now) {
    usb_volume_relay.readback_pending=true;
    usb_volume_relay.readback_after=after;
    usb_volume_relay.readback_since=now;
}
static void bt_volume_command_done(void) {
    if (sink_volume.pending || sink_volume.inflight || usb_volume_relay.host_deferred) return;
    // A SET response contains the actually applied gain. It does not consume
    // the native volume notification subscription; replacing that subscription
    // after every SET stranded AirPods readback. Only CHANGED re-registers it.
    usb_volume_relay.host_pending=false;
}
static void bt_volume_retire_older_request(void) {
    if (sink_volume.request_revision!=usb_volume_relay.change_revision) return;
    sink_volume.pending=false;
    usb_volume_relay.host_pending=usb_volume_relay.host_deferred=usb_volume_relay.host_exact=false;
    host_volume_write=false;
}
static bool bt_volume_change_resolve(void) {
    if (!usb_volume_relay.change_pending || usb_volume_relay.change_wait_ack || !usb_volume_relay.change_interim) return false;
    // A clamped SET echo is not a later gesture; a differing accessory observation is.
    if (!usb_volume_relay.change_accepted || usb_volume_relay.change_level!=usb_volume_relay.change_ack_level)
        bt_volume_retire_older_request();
    sink_volume.confirmed=usb_volume_relay.change_level;
    sink_volume.known=true;
    usb_volume_relay.change_pending=false;
    bt_volume_command_done();
    bt_volume_publish();
    return true;
}
static void bt_volume_relay_poll(uint32_t now) {
    if (usb_volume_relay.change_pending && (uint32_t)(now-usb_volume_relay.readback_since)>=1000u) {
        bt_volume_retire_older_request();
        usb_volume_relay.change_pending=false;
        usb_volume_relay.readback_pending=false;
        sink_volume.known=false;
    }
    if (bt_volume_host_managed()) return;
    if (!aacp_audio_owned()) {
        usb_media_cancel_volume();
        // Only a SET actually received in standby is deferred. Losing authority
        // while an older owned SET awaits ACK must not turn it into a new intent.
        if (!usb_volume_relay.host_deferred) usb_volume_relay.host_pending=false;
        sink_volume.pending=false; // An already-sent command may finish, but must never retry after yield.
        usb_volume_relay.readback_pending=false;
        usb_volume_relay.change_pending=false;
        sink_volume.known=false;
        _bt_sink_volume_changed=false;
        return;
    }
    if (usb_volume_relay.host_deferred && !sink_volume.inflight) {
        usb_volume_relay.host_deferred=false;
        if (usb_volume_relay.set_revision==usb_volume_set_revision() &&
            usb_volume_set_generation()==usb_media_generation() && usb_host_active()) {
            set_bt_volume(bt_volume_to_usb(usb_volume_relay.host_level));
        } else {
            usb_volume_relay.host_pending=false;
        }
    }
    bt_volume_command_done();
    if (usb_volume_relay.readback_pending && (uint32_t)(now-usb_volume_relay.readback_since)>=500u &&
        !usb_volume_relay.change_pending && !sink_volume.pending && !sink_volume.inflight) {
        // An absent fresh INTERIM cannot justify restoring an older physical gain.
        usb_volume_relay.host_pending=usb_volume_relay.readback_pending=false;
        sink_volume.known=false;
    }
}
static void bt_volume_notification(uint16_t cid,uint8_t ctype,uint8_t level,uint32_t epoch,uint8_t label) {
    uint32_t now=btstack_run_loop_get_time_ms();
    bt_volume_trace(VOLUME_TRACE_NOTIFY,ctype,label,level,sink_volume.confirmed,(uint8_t)epoch);
    if (!epoch || level>127 || cid!=sink_volume.cid) return;
    if (!aacp_audio_owned()) {
        if (ctype==AVRCP_CTYPE_RESPONSE_CHANGED_STABLE) ++volume_changed_reports;
        else ++volume_interim_reports;
        sink_volume.known=false;
        return; // Standby observations cannot authorize gain or host-key changes.
    }
    if (ctype==AVRCP_CTYPE_RESPONSE_CHANGED_STABLE) {
        ++volume_changed_reports;
        if (usb_volume_relay.physical_generation && (int32_t)(epoch-usb_volume_relay.physical_generation)<0) return;
        if (usb_volume_relay.changed_generation && (int32_t)(epoch-usb_volume_relay.changed_generation)<=0) return;
        usb_volume_relay.changed_generation=epoch;
        if (sink_volume.pending || sink_volume.inflight || usb_volume_relay.change_pending) {
            usb_volume_relay.change_pending=true;
            usb_volume_relay.change_interim=false;
            usb_volume_relay.change_wait_ack=sink_volume.inflight;
            usb_volume_relay.change_accepted=!sink_volume.inflight && sink_volume.known;
            usb_volume_relay.change_ack_level=sink_volume.confirmed;
            usb_volume_relay.change_revision=sink_volume.request_revision;
            usb_volume_relay.change_epoch=epoch;
        }
        bt_volume_readback(epoch,now);
        return; // AOSP model: CHANGED triggers a fresh INTERIM; its payload is not a gesture.
    }
    ++volume_interim_reports;
    // A registration sent before a newer SET ACK may still contain the old gain.
    if (usb_volume_relay.acknowledged_generation &&
        (int32_t)(epoch-usb_volume_relay.acknowledged_generation)<=0) return;
    if ((usb_volume_relay.physical_generation && (int32_t)(epoch-usb_volume_relay.physical_generation)<=0) ||
        (usb_volume_relay.readback_pending && (int32_t)(epoch-usb_volume_relay.readback_after)<=0)) return;
    // Keep receive order: a later accepted SET ACK supersedes this observation.
    bool known=sink_volume.known;
    uint8_t previous=sink_volume.confirmed;
    if (!volume_sync_notification(&sink_volume,cid,level)) return;
    bt_volume_observed();
    usb_volume_relay.physical_generation=epoch;
    usb_volume_relay.readback_pending=false;
    if (usb_volume_relay.change_pending && (int32_t)(epoch-usb_volume_relay.change_epoch)>0) {
        usb_volume_relay.change_interim=true;
        usb_volume_relay.change_level=level;
        if (bt_volume_change_resolve()) return;
    }
    bt_volume_command_done();
    if (bt_volume_host_managed()) {
        _bt_sink_volume_changed=false;
        return; // Raw readback reaches the host bridge without a duplicate HID step or gain repair.
    }
    // Preserve the accessory's native step; the UAC hardware-volume notification updates the host.
    if (!known || previous!=level) bt_volume_publish();
}
void bt_volume_poll(void) {
    bt_volume_select_peer();
    bt_volume_authority_poll();
    bt_volume_relay_poll(btstack_run_loop_get_time_ms());
    if (!aacp_audio_owned()) {
        uint8_t unused;
        (void)volume_sync_due(&sink_volume,btstack_run_loop_get_time_ms(),&unused);
        return; // Expire an old in-flight receipt without sending anything from standby.
    }
    if (get_usb_streaming() || aacp_mic_requested() || !phone_sink_owns_controls()) bt_volume_cancel_phone();
    bool active=sink_volume.pending || sink_volume.inflight;
    uint32_t now=btstack_run_loop_get_time_ms();
    uint8_t level;
    bool due=volume_sync_due(&sink_volume,now,&level);
    if (usb_volume_relay.change_pending && usb_volume_relay.change_wait_ack && !sink_volume.inflight) {
        usb_volume_relay.change_wait_ack=false;
        usb_volume_relay.change_accepted=false;
        (void)bt_volume_change_resolve();
    }
    if (aacp_audio_owned() && due && sink_volume.pending && !usb_volume_relay.change_pending) {
        uint8_t status=avrcp_controller_set_absolute_volume(sink_volume.cid,level);
        avrcp_connection_t *connection=avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER,sink_volume.cid);
        volume_sync_send(&sink_volume,now,status==ERROR_CODE_SUCCESS && connection!=NULL,connection?connection->transaction_id:0);
        bt_volume_trace(VOLUME_TRACE_TX,0,connection?connection->transaction_id:0,level,sink_volume.confirmed,status);
    }
    if (active && !sink_volume.pending && !sink_volume.inflight) {
        host_volume_write=false;
        bt_volume_command_done();
        bt_volume_publish();
    }
}
bool bt_volume_controller_event(const uint8_t *packet,uint16_t size) {
    if (!packet || size<5 || packet[1]+2u!=size || !sink_volume.cid || little_endian_read_16(packet,3)!=sink_volume.cid) return false;
    avrcp_connection_t *connection=avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER,sink_volume.cid);
    switch(packet[2]) {
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            if (size!=7 || packet[6]>127 || (packet[5]!=AVRCP_CTYPE_RESPONSE_INTERIM && packet[5]!=AVRCP_CTYPE_RESPONSE_CHANGED_STABLE)) break;
            if (connection) bt_volume_notification(sink_volume.cid,packet[5],packet[6],
                avrcp_controller_volume_event_generation(sink_volume.cid),connection->controller_last_confirmed_transaction_id);
            break;
        case AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE:
            if (size!=7 || (packet[5]!=AVRCP_CTYPE_RESPONSE_ACCEPTED && packet[5]!=AVRCP_CTYPE_RESPONSE_REJECTED &&
                packet[5]!=AVRCP_CTYPE_RESPONSE_NOT_IMPLEMENTED)) break;
            bt_volume_trace(VOLUME_TRACE_ACK,packet[5],connection?connection->controller_last_confirmed_transaction_id:0,
                packet[6],sink_volume.confirmed,0);
            bool overlap=usb_volume_relay.change_pending && usb_volume_relay.change_wait_ack;
            if (connection && volume_sync_response(&sink_volume,sink_volume.cid,connection->controller_last_confirmed_transaction_id,
                    packet[5]==AVRCP_CTYPE_RESPONSE_ACCEPTED,packet[6])) {
                if (overlap) {
                    usb_volume_relay.change_wait_ack=false;
                    usb_volume_relay.change_accepted=packet[5]==AVRCP_CTYPE_RESPONSE_ACCEPTED;
                    usb_volume_relay.change_ack_level=packet[6];
                    sink_volume.known=usb_volume_relay.change_interim;
                    if (sink_volume.known) sink_volume.confirmed=usb_volume_relay.change_level;
                }
                if (packet[5]==AVRCP_CTYPE_RESPONSE_ACCEPTED) {
                    if (!overlap) bt_volume_observed();
                    // Fence only an INTERIM still in transit; an already observed gesture keeps its ordering.
                    if (!overlap && connection->controller_volume_active && !connection->controller_volume_interim) {
                        usb_volume_relay.acknowledged_generation=connection->controller_volume_generation;
                        if (usb_volume_relay.readback_pending &&
                            (int32_t)(usb_volume_relay.acknowledged_generation-usb_volume_relay.readback_after)>0)
                            usb_volume_relay.readback_pending=false;
                    }
                    if (bt_volume_host_managed() && (!overlap || usb_volume_relay.change_interim)) {
                        usb_volume_relay.readback_pending=false;
                    }
                }
                if (!sink_volume.pending && !sink_volume.inflight) host_volume_write=false;
                ++volume_ack_reports;bt_volume_command_done();
                if (!aacp_audio_owned()) sink_volume.known=false;
                if (bt_volume_change_resolve()) break;
                bt_volume_publish();
            }
            break;
        default:return false;
    }
    return true;
}
bool bt_volume_target_event(const uint8_t *packet,uint16_t size) {
    if (!packet || size!=7 || packet[1]!=5 || packet[2]!=AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED ||
        packet[5]!=AVRCP_CTYPE_NOTIFY || packet[6]>127 || !sink_volume.selected || !sink_volume.cid ||
        little_endian_read_16(packet,3)!=sink_volume.cid) return false;
    ++volume_target_requests;usb_volume_relay.target_known=true;usb_volume_relay.target_level=packet[6];
    return true; // Source gain intent is observed separately, never physical readback or a duplicate key.
}

bool bridge_phone_volume(uint8_t volume) {
    if (volume > 127 || get_usb_streaming() || aacp_mic_requested()) return false;
    bt_volume_select_peer();
    bool accepted = volume_sync_request(&sink_volume, volume, true, btstack_run_loop_get_time_ms());
    if (accepted && !sink_volume.pending) bt_volume_publish();
    return accepted;
}

bool bridge_phone_get_volume(uint8_t *volume) { return get_bt_volume(volume); }

void increase_vol_by_key(){
    bt_volume_select_peer();
    volume_sync_step(&sink_volume, true, btstack_run_loop_get_time_ms());
}


void decrease_vol_by_key(){

    bt_volume_select_peer();
    volume_sync_step(&sink_volume, false, btstack_run_loop_get_time_ms());
    
}

// getter returns a pointer to that storage
bool * get_is_bt_sink_volume_changed_ptr(void){
    return &_bt_sink_volume_changed;
}

bool bluetooth_volume_peer_matches(const uint8_t address[6]) {
    return sink_volume.selected && !memcmp(address, sink_volume.peer, 6);
}

void bluetooth_volume_connection_changed(uint16_t avrcp_cid) {
    volume_sync_connection(&sink_volume, avrcp_cid, btstack_run_loop_get_time_ms());
    bt_volume_relay_reset(false);
}

void bluetooth_volume_record_operation(uint8_t operation_id) {
    if (operation_id == AVRCP_OPERATION_ID_VOLUME_UP) ++volume_operations_up;
    if (operation_id == AVRCP_OPERATION_ID_VOLUME_DOWN) ++volume_operations_down;
}
