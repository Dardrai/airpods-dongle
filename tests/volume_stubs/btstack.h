// SPDX-License-Identifier: GPL-3.0-only
// Minimal BTstack surface for linking src/btstack/bluetooth_volume.c on the host.
#ifndef TEST_VOLUME_BTSTACK_H
#define TEST_VOLUME_BTSTACK_H
#include <stdbool.h>
#include <stdint.h>
#include "bluetooth.h"

enum {AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED=1,AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE=2,
 AVRCP_CTYPE_RESPONSE_INTERIM=15,AVRCP_CTYPE_RESPONSE_CHANGED_STABLE=13,AVRCP_CTYPE_NOTIFY=3,
 AVRCP_CTYPE_RESPONSE_ACCEPTED=9,AVRCP_CTYPE_RESPONSE_REJECTED=10,AVRCP_CTYPE_RESPONSE_NOT_IMPLEMENTED=8,
 AVRCP_CONTROLLER=0,ERROR_CODE_SUCCESS=0};
enum {AVRCP_OPERATION_ID_VOLUME_UP=0x41,AVRCP_OPERATION_ID_VOLUME_DOWN=0x42};
typedef struct {
 uint8_t transaction_id,controller_last_confirmed_transaction_id;
 uint32_t controller_volume_generation;
 bool controller_volume_active,controller_volume_interim;
} avrcp_connection_t;

uint32_t btstack_run_loop_get_time_ms(void);
uint32_t avrcp_controller_volume_event_generation(uint16_t cid);
uint8_t avrcp_controller_set_absolute_volume(uint16_t cid,uint8_t level);
avrcp_connection_t *avrcp_get_connection_for_avrcp_cid_for_role(unsigned role,uint16_t cid);
static inline uint16_t little_endian_read_16(const uint8_t *p,unsigned i){return (uint16_t)(p[i]|(p[i+1]<<8));}
#endif
