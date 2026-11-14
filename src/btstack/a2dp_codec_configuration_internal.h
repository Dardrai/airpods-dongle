// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_A2DP_CODEC_CONFIGURATION_INTERNAL_H
#define AIRPODS_A2DP_CODEC_CONFIGURATION_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>
#include "btstack.h"

#define AVDTP_MAX_SEP_NUM 10
#define AVDTP_MAX_MEDIA_CODEC_CONFIG_LEN 16
#define AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN 100

// One remote sink stream endpoint and its cached capability event.
typedef struct {
    uint8_t seid;
    avdtp_media_codec_type_t codec_type;
    bool have_media_codec_capabilities;
    bool delay_reporting;
    uint8_t                  media_codec_event[AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN];
    uint32_t                 vendor_id;
    uint32_t                 codec_id;
} a2dp_remote_endpoint_t;

#ifdef HOST_TEST_VISIBILITY
// Host tests observe and seed the catalog; firmware keeps it static.
extern a2dp_remote_endpoint_t remote_seps[AVDTP_MAX_SEP_NUM];
extern uint16_t num_remote_seps;
extern uint16_t cur_num_remote_seps;
extern bool remote_catalog_truncated;
extern uint16_t media_codec_config_len;
extern uint8_t media_codec_config_data[AVDTP_MAX_MEDIA_CODEC_CONFIG_LEN];
extern uint8_t selected_remote_sep_index;
extern bool have_aaceld_codec_capabilities;
extern bool finish_setup_aac;
extern uint8_t media_aaceld_codec_capabilities[14];
extern uint8_t local_stream_endpoint_aaceld_media_codec_configuration[14];
extern avdtp_stream_endpoint_t *stream_endpoint_aaceld;
int find_remote_seid(uint8_t remote_seid);
int setup_aac_configuration(void);
int setup_aaceld_configuration(void);
#endif

#endif
