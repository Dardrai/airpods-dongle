// SPDX-License-Identifier: GPL-3.0-only
#ifndef PHONE_CONTROL_PROTOCOL_H
#define PHONE_CONTROL_PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../tinyusb/dongle_protocol_spec.h"
#define PHONE_CONTROL_FRAME_MAX DONGLE_PHONE_MAXIMUM_FRAME_SIZE
typedef struct { uint8_t bytes[PHONE_CONTROL_FRAME_MAX]; uint8_t used, expected; } phone_control_parser_t;
typedef bool (*phone_control_request_t)(void *context, const uint8_t *frame);
static inline bool phone_control_feed(phone_control_parser_t *p, const uint8_t *data,
    uint16_t size, phone_control_request_t request, void *context) {
    if (!p || (!data && size) || !request) return false;
    for (unsigned at=0; at<size; ++at) {
        p->bytes[p->used++]=data[at];
        if (p->used==DONGLE_PHONE_HEADER_SIZE) {
            const uint8_t *h=p->bytes;
            uint8_t operation=h[DONGLE_PHONE_OFFSET_OPERATION], report=h[DONGLE_PHONE_OFFSET_REPORT_ID];
            uint8_t length=h[DONGLE_PHONE_OFFSET_LENGTH];
            if (memcmp(h+DONGLE_PHONE_OFFSET_MAGIC,DONGLE_MAGIC,DONGLE_MAGIC_SIZE) ||
                h[DONGLE_PHONE_OFFSET_VERSION]!=DONGLE_PROTOCOL_VERSION ||
                (report!=DONGLE_REPORT_ID && report!=DONGLE_RPC_REPORT_ID) ||
                !((operation==DONGLE_PHONE_OPERATION_READ_REPORT && length==0) ||
                  (operation==DONGLE_PHONE_OPERATION_WRITE_REPORT && length==DONGLE_REPORT_SIZE))) return false;
            p->expected=(uint8_t)(DONGLE_PHONE_HEADER_SIZE+length);
        }
        if (p->expected && p->used==p->expected) {
            // A written report must carry the same report ID and a valid frame header.
            const uint8_t *r=p->bytes+DONGLE_PHONE_HEADER_SIZE;
            if (p->bytes[DONGLE_PHONE_OFFSET_LENGTH] &&
                (r[DONGLE_HEADER_OFFSET_REPORT_ID]!=p->bytes[DONGLE_PHONE_OFFSET_REPORT_ID] ||
                 memcmp(r+DONGLE_HEADER_OFFSET_MAGIC,DONGLE_MAGIC,DONGLE_MAGIC_SIZE) ||
                 r[DONGLE_HEADER_OFFSET_VERSION]!=DONGLE_PROTOCOL_VERSION ||
                 r[DONGLE_HEADER_OFFSET_LENGTH]!=DONGLE_REPORT_SIZE)) return false;
            if (!request(context,p->bytes)) return false;
            p->used=p->expected=0;
        }
    }
    return true;
}
#endif
