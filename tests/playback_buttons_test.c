// SPDX-License-Identifier: GPL-3.0-only
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../src/tinyusb/media_control.h"
#include "btstack.h"
#include "../src/tinyusb/uac.h"
#include "../src/btstack/btstack_aacp.h"
#include "../src/btstack/btstack_avdtp_source.h"
#include "../src/btstack/phone_sink.h"
#include "../src/btstack/phone_call.h"
#include "../src/btstack/bluetooth_volume.h"
#include "../src/btstack/avdtp_source_session.h"
// The production unit src/btstack/avrcp_connection.c is linked; its state is test-visible.
#include "../src/btstack/avrcp_connection_internal.h"
static bool is_usb_streaming=true,phone_controls,mic;
static bool audio_owned=true;
bool aacp_audio_owned(void) {return audio_owned;}
bool get_usb_streaming(void) {return is_usb_streaming;}
static unsigned manual_operations;
void playback_accessory_cancel_automatic_media(void) {++manual_operations;}
static unsigned usb_events,phone_events;
static uint32_t volume_operations_up,volume_operations_down;
static unsigned target_requests;
static enum usb_media_command last;
bool phone_sink_owns_controls(void) {return phone_controls;}
bool usb_mic_requested(void) {return mic;}
bool aacp_mic_requested(void) {return mic;}
static bool call_controls;
static unsigned call_events;
bool phone_call_media_control(uint8_t operation) {(void)operation;if(call_controls)++call_events;return call_controls;}
bool phone_sink_media_control(uint8_t operation) {(void)operation;++phone_events;return true;}
void bluetooth_volume_record_operation(uint8_t operation) {
    if(operation==AVRCP_OPERATION_ID_VOLUME_UP)++volume_operations_up;
    if(operation==AVRCP_OPERATION_ID_VOLUME_DOWN)++volume_operations_down;
}
bool bt_volume_target_event(const uint8_t *packet,uint16_t size) {
    if(size!=7 || packet[5]!=3 || packet[6]>127)return false;
    ++target_requests;return true;
}
bool usb_media_enqueue(enum usb_media_command command) {last=command;++usb_events;return true;}
int main(void) {
    avrcp_cid=7;
    // The pinned emitter sets length to 17 while delivering eight bytes for zero operands.
    uint8_t event[9]={HCI_EVENT_AVRCP_META,17,AVRCP_SUBEVENT_OPERATION,7,0,AVRCP_OPERATION_ID_PAUSE,1,0,0};
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);
    assert(usb_events==1 && last==USB_MEDIA_PAUSE);
    assert(manual_operations==1);
    event[6]=0; avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8); assert(usb_events==1);
    event[6]=1; event[5]=AVRCP_OPERATION_ID_PLAY; event[1]=6;
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8); assert(usb_events==2 && last==USB_MEDIA_PLAY);
    event[7]=1; avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8); assert(usb_events==2);
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,7); assert(usb_events==2);
    event[7]=0;event[1]=5;avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(usb_events==2);
    event[1]=17;event[3]=8;avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(usb_events==2);
    event[3]=7;phone_controls=true;is_usb_streaming=false;
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(phone_events==1 && usb_events==2);
    is_usb_streaming=true;
    event[5]=AVRCP_OPERATION_ID_FORWARD;avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);
    assert(usb_events==3 && last==USB_MEDIA_NEXT);
    is_usb_streaming=false;call_controls=true;event[5]=AVRCP_OPERATION_ID_PLAY;
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(call_events==1 && phone_events==1);
    event[3]=8;avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(call_events==1);
    event[3]=7;is_usb_streaming=true;
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(call_events==1 && usb_events==4);
    event[5]=AVRCP_OPERATION_ID_VOLUME_UP;event[6]=0;
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(!volume_operations_up);
    event[6]=1;event[3]=8;
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(!volume_operations_up);
    event[3]=7;avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(volume_operations_up==1);
    event[5]=AVRCP_OPERATION_ID_VOLUME_DOWN;
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(volume_operations_down==1 && usb_events==4);
    uint8_t volume[]={HCI_EVENT_AVRCP_META,5,AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED,7,0,3,47};
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,volume,7);assert(target_requests==1 && usb_events==4);
    volume[3]=8;avrcp_target_packet_handler(HCI_EVENT_PACKET,0,volume,7);assert(target_requests==1);
    audio_owned=false;event[3]=7;event[5]=AVRCP_OPERATION_ID_PLAY;
    avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);assert(usb_events==4 && call_events==1);
    is_usb_streaming=false;avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);
    assert(phone_events==1 && call_events==1);
    event[5]=AVRCP_OPERATION_ID_VOLUME_DOWN;avrcp_target_packet_handler(HCI_EVENT_PACKET,0,event,8);
    assert(volume_operations_down==1);
    puts("stem buttons: actual SDK callback length, press/release, stale CID and USB/phone/call ownership passed");
}
