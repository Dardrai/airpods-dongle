// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_VOLUME_AUTHORITY_H
#define AIRPODS_VOLUME_AUTHORITY_H
#include "peer_address.h"
#include "../tinyusb/dongle_client.h"
#include "../tinyusb/dongle_protocol.h"

#define VOLUME_AUTHORITY_TTL_MS ((uint32_t)DONGLE_VOLUME_AUTHORITY_LEASE_MILLISECONDS)
#define VOLUME_AUTHORITY_SNAPSHOT_SIZE DONGLE_PHYSICAL_VOLUME_SIZE

typedef struct {
    uint32_t usb_generation, control_generation;
    uint16_t cid;
    uint8_t peer[6];
    bool ready;
} volume_authority_scope_t;

typedef struct {
    volume_authority_scope_t scope;
    uint32_t serial, token, deadline;
    uint8_t client;
    bool active;
} volume_authority_t;

static inline bool volume_authority_scope_ready(const volume_authority_scope_t *scope) {
    return scope && scope->ready && (scope->usb_generation & 1u) &&
        scope->control_generation && scope->cid && peer_address_valid(scope->peer);
}

static inline bool volume_authority_current(const volume_authority_t *s,
                                            const volume_authority_scope_t *scope,
                                            uint32_t now) {
    return s->active && volume_authority_scope_ready(scope) &&
        s->scope.usb_generation==scope->usb_generation &&
        s->scope.control_generation==scope->control_generation &&
        s->scope.cid==scope->cid && !memcmp(s->scope.peer,scope->peer,6) &&
        (int32_t)(now-s->deadline)<0;
}

static inline bool volume_authority_owner(const volume_authority_t *s,uint8_t client,
                                          const volume_authority_scope_t *scope,
                                          uint32_t now) {
    return client==DONGLE_CLIENT_USB && s->client==client &&
        volume_authority_current(s,scope,now);
}

static inline uint32_t volume_authority_remaining(const volume_authority_t *s,
                                                 const volume_authority_scope_t *scope,
                                                 uint32_t now) {
    return volume_authority_current(s,scope,now) ? s->deadline-now : 0;
}

// The caller retires authority-origin writes when this reports a revoked scope.
static inline bool volume_authority_poll(volume_authority_t *s,
                                         const volume_authority_scope_t *scope,
                                         uint32_t now) {
    if (!s->active || volume_authority_current(s,scope,now)) return false;
    s->active=false; s->token=0;
    return true;
}

static inline bool volume_authority_disconnect(volume_authority_t *s,uint8_t client) {
    if (!s->active || s->client!=client) return false;
    s->active=false; s->token=0;
    return true;
}

// Acquire carries no token; every other action carries one. Only set_level carries a level.
static inline bool volume_authority_arguments_valid(uint8_t action,uint32_t token,uint8_t level) {
    return action<=DONGLE_VOLUME_AUTHORITY_ACTION_SET_LEVEL && level<=DONGLE_PHYSICAL_VOLUME_MAXIMUM_LEVEL &&
        (action==DONGLE_VOLUME_AUTHORITY_ACTION_SET_LEVEL || !level) &&
        (action==DONGLE_VOLUME_AUTHORITY_ACTION_ACQUIRE ? !token : token!=0);
}

// The whole feature 17 request: size, feature byte and arguments.
static inline bool volume_authority_request_valid(const uint8_t *p,uint16_t size) {
    return p && size==DONGLE_VOLUME_AUTHORITY_REQUEST_SIZE &&
        p[DONGLE_VOLUME_AUTHORITY_OFFSET_FEATURE]==DONGLE_FEATURE_USB_HOST_VOLUME_AUTHORITY &&
        volume_authority_arguments_valid(p[DONGLE_VOLUME_AUTHORITY_OFFSET_ACTION],
            dongle_u32(p+DONGLE_VOLUME_AUTHORITY_OFFSET_TOKEN),p[DONGLE_VOLUME_AUTHORITY_OFFSET_LEVEL]);
}

// Poll revocation before managing a valid request; malformed requests change nothing.
static inline uint8_t volume_authority_manage(volume_authority_t *s,uint8_t client,
                                              const volume_authority_scope_t *scope,
                                              uint8_t action,uint32_t token,uint8_t level,
                                              uint32_t now) {
    if (!volume_authority_arguments_valid(action,token,level)) return DONGLE_MALFORMED;
    if (client!=DONGLE_CLIENT_USB) return DONGLE_UNSUPPORTED;
    if (!volume_authority_scope_ready(scope)) return DONGLE_DISCONNECTED;
    if (action==DONGLE_VOLUME_AUTHORITY_ACTION_ACQUIRE) {
        if (s->active) return DONGLE_BUSY;
        if (!++s->serial) ++s->serial;
        s->scope=*scope; s->client=client; s->token=s->serial;
        s->deadline=now+VOLUME_AUTHORITY_TTL_MS; s->active=true;
        return DONGLE_ACCEPTED;
    }
    if (!volume_authority_owner(s,client,scope,now) || token!=s->token) return DONGLE_STALE;
    if (action==DONGLE_VOLUME_AUTHORITY_ACTION_RELEASE) { s->active=false; s->token=0; }
    else s->deadline=now+VOLUME_AUTHORITY_TTL_MS;
    return DONGLE_ACCEPTED;
}
#endif
