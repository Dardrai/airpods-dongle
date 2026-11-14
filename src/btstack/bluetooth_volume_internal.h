// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_BLUETOOTH_VOLUME_INTERNAL_H
#define AIRPODS_BLUETOOTH_VOLUME_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>
#include "volume_sync.h"
#include "volume_authority.h"

// USB host volume relay to the selected AirPods; owned by bluetooth_volume.c.
typedef struct {
    uint32_t generation, set_revision;
    uint32_t readback_after, readback_since, physical_generation, acknowledged_generation;
    uint32_t changed_generation, change_revision, change_epoch;
    uint16_t cid;
    uint8_t target_level, host_level;
    bool host_pending, target_known, readback_pending;
    bool host_deferred, host_exact;
    uint8_t change_level, change_ack_level;
    bool change_pending, change_interim, change_wait_ack, change_accepted;
} bluetooth_volume_relay_t;

#ifdef HOST_TEST_VISIBILITY
// Host tests observe and reset the module state; firmware keeps it static.
extern volume_sync_t sink_volume;
extern uint32_t volume_operations_up, volume_operations_down;
extern uint32_t volume_changed_reports, volume_interim_reports, volume_ack_reports;
extern uint32_t volume_target_requests;
extern bluetooth_volume_relay_t usb_volume_relay;
extern volume_authority_t usb_host_volume;
extern uint32_t volume_observation_revision;
extern bool host_volume_write;
extern bool host_volume_quiet;
extern bool _bt_sink_volume_changed;
volume_authority_scope_t bt_volume_authority_scope(void);
void bt_volume_relay_reset(bool peer_changed);
#endif

#endif
