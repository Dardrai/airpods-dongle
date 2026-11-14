// SPDX-License-Identifier: GPL-3.0-only
// Minimal BTstack surface for linking src/btstack/avrcp_connection.c on the host.
#ifndef TEST_AVRCP_BTSTACK_H
#define TEST_AVRCP_BTSTACK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bluetooth.h"

#define UNUSED(x) (void)(x)
#define ERROR_CODE_SUCCESS 0
#define HCI_EVENT_PACKET 4
#define HCI_EVENT_AVRCP_META 0xe1
#define AVRCP_SUBEVENT_CONNECTION_ESTABLISHED 1
#define AVRCP_SUBEVENT_CONNECTION_RELEASED 2
#define AVRCP_SUBEVENT_PLAY_STATUS_QUERY 3
#define AVRCP_SUBEVENT_OPERATION 4
#define AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED 5
#define AVRCP_SUBEVENT_NOTIFICATION_EVENT_BATT_STATUS_CHANGED 6
#define AVRCP_SUBEVENT_NOTIFICATION_STATE 7
#define AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED 1
#define AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED 2
#define AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED 9
#define AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED 13
#define AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED 6
#define AVRCP_MAX_COMMAND_PARAMETER_LENGTH 11
#define AVRCP_OPERATION_ID_PLAY 0x44
#define AVRCP_OPERATION_ID_PAUSE 0x46
#define AVRCP_OPERATION_ID_FORWARD 0x4b
#define AVRCP_OPERATION_ID_BACKWARD 0x4c
#define AVRCP_OPERATION_ID_STOP 0x45
#define AVRCP_OPERATION_ID_VOLUME_UP 0x41
#define AVRCP_OPERATION_ID_VOLUME_DOWN 0x42
#define AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER 1
#define AVRCP_FEATURE_MASK_CATEGORY_MONITOR_OR_AMPLIFIER 2

typedef uint8_t avrcp_operation_id_t;
typedef uint8_t avrcp_playback_status_t;
typedef void (*btstack_packet_handler_t)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
typedef struct {
    uint8_t track_id[8];
    uint16_t track_nr;
    char *title, *artist, *album, *genre;
    uint32_t song_length_ms; // The production table initializes through song_length_ms.
} avrcp_track_t;

uint32_t btstack_run_loop_get_time_ms(void);
const char *bd_addr_to_str(const bd_addr_t address);
uint8_t avrcp_connect(bd_addr_t address, uint16_t *avrcp_cid);
uint8_t avrcp_disconnect(uint16_t avrcp_cid);
void avrcp_init(void);
void avrcp_register_packet_handler(btstack_packet_handler_t handler);
void avrcp_target_init(void);
void avrcp_target_register_packet_handler(btstack_packet_handler_t handler);
void avrcp_controller_init(void);
void avrcp_controller_register_packet_handler(btstack_packet_handler_t handler);
void avrcp_target_create_sdp_record(uint8_t *service, uint32_t handle, uint16_t features,
                                    const char *name, const char *provider);
void avrcp_controller_create_sdp_record(uint8_t *service, uint32_t handle, uint16_t features,
                                        const char *name, const char *provider);
uint8_t sdp_register_service(const uint8_t *record);
uint8_t avrcp_target_support_event(uint16_t avrcp_cid, uint8_t event);
uint8_t avrcp_target_set_now_playing_info(uint16_t avrcp_cid, const avrcp_track_t *track, uint16_t total_tracks);
uint8_t avrcp_controller_enable_notification(uint16_t avrcp_cid, uint8_t event);
uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t length_ms, uint32_t position_ms,
                                 avrcp_playback_status_t status);
const char *avrcp_operation2str(uint8_t operation);
const char *avrcp_event2str(uint8_t event);

static inline uint8_t hci_event_packet_get_type(const uint8_t *p) {return p[0];}
static inline uint16_t little_endian_read_16(const uint8_t *p,unsigned at) {return (uint16_t)(p[at]|(uint16_t)p[at+1]<<8);}
static inline uint16_t avrcp_subevent_connection_established_get_avrcp_cid(const uint8_t *p) {return little_endian_read_16(p,4);}
static inline uint8_t avrcp_subevent_connection_established_get_status(const uint8_t *p) {return p[3];}
static inline void avrcp_subevent_connection_established_get_bd_addr(const uint8_t *p,bd_addr_t addr) {
    for (unsigned i=0;i<6;++i)addr[i]=p[11-i];
}
static inline uint16_t avrcp_subevent_connection_released_get_avrcp_cid(const uint8_t *p) {return little_endian_read_16(p,3);}
static inline uint8_t avrcp_subevent_operation_get_operation_id(const uint8_t *p) {return p[5];}
static inline uint8_t avrcp_subevent_operation_get_button_pressed(const uint8_t *p) {return p[6];}
static inline uint8_t avrcp_subevent_notification_event_batt_status_changed_get_battery_status(const uint8_t *p) {return p[6];}
static inline uint8_t avrcp_subevent_notification_state_get_event_id(const uint8_t *p) {return p[6];}
static inline uint8_t avrcp_subevent_notification_state_get_enabled(const uint8_t *p) {return p[7];}
#endif
