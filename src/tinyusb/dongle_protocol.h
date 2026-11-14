// SPDX-License-Identifier: GPL-3.0-only
#ifndef DONGLE_PROTOCOL_H
#define DONGLE_PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "dongle_protocol_spec.h"

// Short names for the report-2 result codes used throughout the firmware.
enum { DONGLE_ACCEPTED = DONGLE_RESULT_ACCEPTED, DONGLE_BUSY = DONGLE_RESULT_BUSY,
       DONGLE_STALE = DONGLE_RESULT_STALE, DONGLE_UNSUPPORTED = DONGLE_RESULT_UNSUPPORTED,
       DONGLE_DISCONNECTED = DONGLE_RESULT_DISCONNECTED, DONGLE_MALFORMED = DONGLE_RESULT_MALFORMED,
       DONGLE_TRANSPORT = DONGLE_RESULT_TRANSPORT };
static inline uint32_t dongle_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline void dongle_put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}
static inline void dongle_header(uint8_t *p) {
    memset(p, 0, DONGLE_REPORT_SIZE);
    p[DONGLE_HEADER_REPORT_ID_OFFSET] = DONGLE_REPORT_ID;
    memcpy(p + DONGLE_HEADER_MAGIC_OFFSET, DONGLE_MAGIC, DONGLE_MAGIC_SIZE);
    p[DONGLE_HEADER_VERSION_OFFSET] = DONGLE_PROTOCOL_VERSION;
    p[DONGLE_HEADER_LENGTH_OFFSET] = DONGLE_REPORT_SIZE;
}
static inline bool dongle_command_valid(const uint8_t *p, unsigned size) {
    if (size != DONGLE_REPORT_SIZE || p[DONGLE_HEADER_REPORT_ID_OFFSET] != DONGLE_REPORT_ID ||
        memcmp(p + DONGLE_HEADER_MAGIC_OFFSET, DONGLE_MAGIC, DONGLE_MAGIC_SIZE) ||
        p[DONGLE_HEADER_VERSION_OFFSET] != DONGLE_PROTOCOL_VERSION || p[DONGLE_HEADER_LENGTH_OFFSET] != size ||
        !dongle_u32(p + DONGLE_CMD_SEQUENCE_OFFSET)) return false;
    for (unsigned i = DONGLE_CMD_RESERVED_OFFSET; i < size; ++i) if (p[i]) return false;
    uint8_t opcode = p[DONGLE_CMD_OPCODE_OFFSET], argument = p[DONGLE_CMD_ARGUMENT_OFFSET];
    if (opcode == DONGLE_OP_LISTENING_MODE)
        return argument == DONGLE_LISTENING_ANC || argument == DONGLE_LISTENING_TRANSPARENCY;
    return opcode >= DONGLE_OP_RECONNECT && opcode <= DONGLE_OP_PHONE_PAIRING && argument == 0;
}
// Report-3 request header: fixed framing, zero phase, bounded payload, zero padding.
static inline bool dongle_rpc_header_valid(const uint8_t *p) {
    if (p[DONGLE_HEADER_REPORT_ID_OFFSET] != DONGLE_RPC_REPORT_ID ||
        memcmp(p + DONGLE_HEADER_MAGIC_OFFSET, DONGLE_MAGIC, DONGLE_MAGIC_SIZE) ||
        p[DONGLE_HEADER_VERSION_OFFSET] != DONGLE_PROTOCOL_VERSION ||
        p[DONGLE_HEADER_LENGTH_OFFSET] != DONGLE_REPORT_SIZE ||
        p[DONGLE_RPC_LENGTH_OFFSET] > DONGLE_RPC_CHUNK || p[DONGLE_RPC_PHASE_OFFSET]) return false;
    for (unsigned i = DONGLE_RPC_PAYLOAD_OFFSET + p[DONGLE_RPC_LENGTH_OFFSET]; i < DONGLE_REPORT_SIZE; i++)
        if (p[i]) return false;
    return true;
}

// Each component retains its own age; omitted slots are not refreshed.
typedef struct {
    uint8_t anc, battery[3], charging;
    uint64_t updated[3];
    bool known[3];
} dongle_readback_t;
static inline void dongle_readback_clear(dongle_readback_t *s) {
    memset(s, 0, sizeof(*s));
    memset(s->battery, DONGLE_BATTERY_UNKNOWN, sizeof(s->battery));
}
static inline uint32_t dongle_age_ms(uint64_t now, uint64_t updated) {
    uint64_t age=now-updated;
    // DONGLE_AGE_UNKNOWN means unknown; known old observations must not wrap to fresh.
    return age>=DONGLE_AGE_UNKNOWN ? DONGLE_AGE_SATURATED : (uint32_t)age;
}
static inline uint32_t dongle_readback_age(const dongle_readback_t *s, unsigned slot,
                                         uint64_t now) {
    return s->known[slot] ? dongle_age_ms(now,s->updated[slot]) : DONGLE_AGE_UNKNOWN;
}
static inline void dongle_readback_packet(dongle_readback_t *s, const uint8_t *p,
                                         unsigned size, uint64_t now) {
    if (size < 7 || memcmp(p, AACP_HEADER, AACP_HEADER_SIZE)) return;
    if (size == AACP_CONTROL_PACKET_SIZE && p[AACP_OPCODE_OFFSET] == AACP_OP_CONTROL && p[5] == 0 &&
        p[AACP_CONTROL_ID_OFFSET] == DONGLE_SETTING_LISTENING_MODE &&
        dongle_u32(p + AACP_CONTROL_VALUE_OFFSET) >= DONGLE_LISTENING_OFF &&
        dongle_u32(p + AACP_CONTROL_VALUE_OFFSET) <= DONGLE_LISTENING_ADAPTIVE)
        s->anc = p[AACP_CONTROL_VALUE_OFFSET];
    const uint8_t count = p[AACP_BATTERY_COUNT_OFFSET];
    if (p[AACP_OPCODE_OFFSET] != AACP_OP_BATTERY || p[5] || !count ||
        size < AACP_BATTERY_ENTRIES_OFFSET + count * AACP_BATTERY_ENTRY_SIZE) return;
    // Validate the entire battery report before changing any slot.
    for (unsigned i = 0; i < count; ++i) {
        uint8_t level = p[AACP_BATTERY_ENTRIES_OFFSET + i * AACP_BATTERY_ENTRY_SIZE + AACP_BATTERY_LEVEL_OFFSET];
        if (level > 100 && level != AACP_BATTERY_LEVEL_UNKNOWN) return;
    }
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *b = p + AACP_BATTERY_ENTRIES_OFFSET + i * AACP_BATTERY_ENTRY_SIZE;
        uint8_t component = b[AACP_BATTERY_COMPONENT_OFFSET], status = b[AACP_BATTERY_STATUS_OFFSET];
        int slot = component == AACP_BATTERY_LEFT ? DONGLE_COMPONENT_LEFT :
                   component == AACP_BATTERY_RIGHT ? DONGLE_COMPONENT_RIGHT :
                   component == AACP_BATTERY_CASE ? DONGLE_COMPONENT_CASE : -1;
        if (slot < 0) continue;
        s->battery[slot] = status == AACP_BATTERY_STATUS_DISCONNECTED ? DONGLE_BATTERY_UNKNOWN :
                           b[AACP_BATTERY_LEVEL_OFFSET];
        s->charging &= (uint8_t)~(1u << slot);
        if (s->battery[slot] <= 100 && (status == AACP_BATTERY_STATUS_CHARGING ||
                                        status == AACP_BATTERY_STATUS_OPTIMIZED_CHARGING))
            s->charging |= 1u << slot;
        s->updated[slot] = now;
        s->known[slot] = true;
    }
}
#endif
