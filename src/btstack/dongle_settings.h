// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_DONGLE_SETTINGS_H
#define AIRPODS_DONGLE_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../tinyusb/dongle_protocol.h"

// The catalog (setting_ids), DONGLE_SETTING_COUNT and per-model profiles are generated
// from protocol/dongle-protocol.json; catalog indices are model-mask bit positions.
#include "dongle_settings_spec.h"
typedef struct { uint32_t value, updated, revision; bool known; } setting_report_t;
typedef struct {
    setting_report_t reports[DONGLE_SETTING_COUNT];
    const setting_profile_t *profile;
    uint8_t firmware_major;
    int8_t allow_off_requested;
} dongle_settings_t;

static inline int setting_index(uint8_t id) {
    for (unsigned i=0;i<DONGLE_SETTING_COUNT;++i) if (setting_ids[i]==id) return (int)i;
    return -1;
}
static inline bool setting_value_valid(uint8_t id, uint32_t value) {
    return dongle_setting_value_valid(id, value);
}
static inline bool setting_explicit_choice(uint8_t id) {
    return dongle_setting_writable_without_report(id);
}
static inline void settings_reset(dongle_settings_t *s) {
    memset(s,0,sizeof(*s)); s->allow_off_requested=-1;
}
static inline uint8_t setting_firmware_major(const uint8_t *p, unsigned n) {
    unsigned i=0, major=0;
    while(i<n && p[i]>='0' && p[i]<='9') {
        if (major>99) return 0;
        major=major*10+p[i++]-'0';
    }
    if (!i || major<1 || major>99) return 0;
    if (i<n && p[i]>='A' && p[i]<='Z') {
        ++i; unsigned start=i;
        while(i<n && p[i]>='0' && p[i]<='9') ++i;
        return i>start && (i==n || (i+1==n && p[i]>='a' && p[i]<='z')) ? (uint8_t)major : 0;
    }
    if (i>=n || p[i++]!='.') return 0;
    for (unsigned part=0;part<2;++part) {
        unsigned value=0, start=i;
        while(i<n && p[i]>='0' && p[i]<='9') {
            value=value*10+p[i++]-'0';
            if (value>255) return 0;
        }
        if (i==start || (part==0 && (i>=n || p[i++]!='.'))) return 0;
    }
    return i==n ? (uint8_t)major : 0;
}
static inline uint8_t setting_packed_major(const uint8_t *p, unsigned n) {
    uint32_t value=0;
    if (!n) return 0;
    for (unsigned i=0;i<n;++i) {
        if (p[i]<'0' || p[i]>'9' || value>(UINT32_MAX-(p[i]-'0'))/10u) return 0;
        value=value*10u+p[i]-'0';
    }
    unsigned major=value>>20;
    return (value&15)==0 && major>=1 && major<=99 ? (uint8_t)major : 0;
}
static inline void settings_metadata(dongle_settings_t *s, const uint8_t *p, unsigned n) {
    const setting_profile_t *previous=s->profile;
    int8_t requested=s->allow_off_requested;
    s->profile=NULL; s->firmware_major=0; s->allow_off_requested=-1;
    if (n<11 || n>384 || p[AACP_OFFSET_OPCODE]!=AACP_OPCODE_DEVICE_METADATA || p[5]) return;
    const uint8_t *fields[11]; unsigned lengths[11], offset=11;
    for (unsigned i=0;i<11;++i) {
        if (offset>=n) return;
        const uint8_t *end=memchr(p+offset,0,n-offset);
        if (!end) return;
        fields[i]=p+offset; lengths[i]=(unsigned)(end-(p+offset));
        for (unsigned j=0;j<lengths[i];++j) if (fields[i][j]<32 || fields[i][j]==127) return;
        offset+=(unsigned)lengths[i]+1;
    }
    if (lengths[1]!=5 || fields[1][0]!='A') return;
    unsigned model=0;
    for (unsigned i=1;i<5;++i) {
        if (fields[1][i]<'0' || fields[1][i]>'9') return;
        model=model*10+fields[1][i]-'0';
    }
    for (unsigned i=0;i<sizeof(setting_profiles)/sizeof(*setting_profiles);++i)
        for (unsigned j=0;j<3;++j)
            if (model && setting_profiles[i].models[j]==model) s->profile=&setting_profiles[i];
    if (s->profile && s->profile==previous) s->allow_off_requested=requested;
    s->firmware_major=setting_firmware_major(fields[10],lengths[10]);
    if (!s->firmware_major) s->firmware_major=setting_firmware_major(fields[4],lengths[4]);
    if (!s->firmware_major) s->firmware_major=setting_packed_major(fields[10],lengths[10]);
}
static inline void settings_packet(dongle_settings_t *s, const uint8_t *p, unsigned n, uint32_t now) {
    if (!p || n<6 || memcmp(p,AACP_HEADER,AACP_HEADER_SIZE) || p[5]) return;
    if (p[AACP_OFFSET_OPCODE]==AACP_OPCODE_DEVICE_METADATA) { settings_metadata(s,p,n); return; }
    if (p[AACP_OFFSET_OPCODE]!=AACP_OPCODE_CONTROL_COMMAND || n<7) return;
    uint8_t id=p[AACP_CONTROL_COMMAND_OFFSET_IDENTIFIER];
    int index=setting_index(id);
    if (index<0) return;
    setting_report_t *r=&s->reports[index];
    if (++r->revision==0) ++r->revision;
    r->known=false;
    if (n!=AACP_CONTROL_COMMAND_SIZE) return;
    uint32_t value=dongle_u32(p+AACP_CONTROL_COMMAND_OFFSET_VALUE);
    // Upper bytes hold previous actions, not commands.
    if (id==DONGLE_SETTING_HOLD_ACTIONS) value&=DONGLE_SETTING_HOLD_ACTIONS_REPORT_MASK;
    if (!setting_value_valid(id,value)) return;
    if (id==DONGLE_SETTING_ALLOW_OFF) s->allow_off_requested=-1;
    r->value=value; r->updated=now; r->known=true;
}
static inline bool setting_report_is(const dongle_settings_t *s, uint8_t id, uint32_t value) {
    int index=setting_index(id);
    return index>=0 && s->reports[index].known && s->reports[index].value==value;
}
static inline bool settings_allow_off(const dongle_settings_t *s) {
    if (s->profile) {
        if (!s->profile->listening) return false;
        if (!s->profile->allow_off_option)
            return !setting_report_is(s,DONGLE_SETTING_ALLOW_OFF,DONGLE_SETTING_ALLOW_OFF_OFF);
    }
    if (s->allow_off_requested>=0) return s->allow_off_requested!=0;
    if (setting_report_is(s,DONGLE_SETTING_ALLOW_OFF,DONGLE_SETTING_ALLOW_OFF_ON)) return true;
    if (setting_report_is(s,DONGLE_SETTING_ALLOW_OFF,DONGLE_SETTING_ALLOW_OFF_OFF)) return false;
    return setting_report_is(s,DONGLE_SETTING_LISTENING_MODE,DONGLE_SETTING_LISTENING_MODE_OFF) ||
        (s->profile && s->firmware_major>=1 && s->firmware_major<=6);
}
static inline bool settings_supported(const dongle_settings_t *s, uint8_t id, uint32_t value) {
    int index=setting_index(id);
    if (index<0 || !setting_value_valid(id,value)) return false;
    bool supported=s->profile ? (s->profile->controls&(UINT32_C(1)<<index))!=0 :
        dongle_setting_writable_with_unknown_model(id);
    if (!supported) return false;
    bool adaptive=s->profile && s->profile->adaptive && s->firmware_major>=6;
    if (id==DONGLE_SETTING_HOLD_ACTIONS && s->profile && !s->profile->listening &&
        ((value&255)==DONGLE_SETTING_HOLD_ACTIONS_NOISE_CONTROL ||
         ((value>>8)&255)==DONGLE_SETTING_HOLD_ACTIONS_NOISE_CONTROL)) return false;
    if (id==DONGLE_SETTING_LISTENING_MODE &&
        ((value==DONGLE_SETTING_LISTENING_MODE_ADAPTIVE && !adaptive) ||
         (value==DONGLE_SETTING_LISTENING_MODE_OFF && !settings_allow_off(s)))) return false;
    if (id==DONGLE_SETTING_HOLD_MODES &&
        (((value&DONGLE_SETTING_HOLD_MODES_ADAPTIVE) && !adaptive) ||
         ((value&DONGLE_SETTING_HOLD_MODES_OFF) && !settings_allow_off(s)))) return false;
    if ((id==DONGLE_SETTING_ADAPTIVE_STRENGTH && !adaptive) ||
        (id==DONGLE_SETTING_PAUSE_WHEN_ASLEEP && s->firmware_major<8)) return false;
    return s->reports[index].known || (s->profile && setting_explicit_choice(id)) ||
        (id==DONGLE_SETTING_ALLOW_OFF && s->profile && s->profile->silent_allow_off);
}
static inline uint16_t settings_snapshot(const dongle_settings_t *s, uint32_t now,
                                         uint8_t *out, uint16_t capacity, bool extended) {
    unsigned limit=extended ? DONGLE_SETTING_COUNT : DONGLE_LEGACY_SETTING_COUNT;
    unsigned count=0;
    for (unsigned i=0;i<limit;++i) if (s->reports[i].known) ++count;
    if (!out || capacity<DONGLE_SETTINGS_SNAPSHOT_HEADER_SIZE+count*DONGLE_SETTINGS_SNAPSHOT_RECORD_SIZE) return 0;
    out[0]=DONGLE_SETTINGS_SNAPSHOT_VERSION; out[1]=(uint8_t)count;
    unsigned offset=DONGLE_SETTINGS_SNAPSHOT_HEADER_SIZE;
    for (unsigned i=0;i<limit;++i) {
        const setting_report_t *r=&s->reports[i];
        if (!r->known) continue;
        out[offset+DONGLE_SETTINGS_RECORD_OFFSET_IDENTIFIER]=setting_ids[i];
        dongle_put32(out+offset+DONGLE_SETTINGS_RECORD_OFFSET_AGE_MILLISECONDS,now-r->updated);
        dongle_put32(out+offset+DONGLE_SETTINGS_RECORD_OFFSET_REVISION,r->revision);
        dongle_put32(out+offset+DONGLE_SETTINGS_RECORD_OFFSET_VALUE,r->value);
        offset+=DONGLE_SETTINGS_SNAPSHOT_RECORD_SIZE;
    }
    return (uint16_t)offset;
}
#endif
