// SPDX-License-Identifier: GPL-3.0-only
// Minimal BTstack surface for linking src/btstack/a2dp_codec_configuration.c on the host.
// Weak defaults for every function live in codec_link_defaults.c.
#ifndef TEST_CODEC_BTSTACK_H
#define TEST_CODEC_BTSTACK_H
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "bluetooth.h"

#define btstack_assert assert
#define ERROR_CODE_SUCCESS 0
enum {AVDTP_AUDIO=0};
enum {AVDTP_SINK=1};
enum {AVDTP_MEDIA_CODEC=7, AVDTP_DELAY_REPORTING=8};
enum {AVDTP_CODEC_SBC=0, AVDTP_CODEC_MPEG_1_2_AUDIO=1, AVDTP_CODEC_MPEG_2_4_AAC=2,
      AVDTP_CODEC_ATRAC_FAMILY=4, AVDTP_CODEC_NON_A2DP=0xff};
enum {AVDTP_AAC_MPEG2_LC=0x80, AVDTP_AAC_MPEG4_LC=0x40};
enum {AVDTP_CHANNEL_MODE_MONO=1, AVDTP_SBC_STEREO=2};
enum {AVDTP_SUBEVENT_SIGNALING_SEP_FOUND=0x20, AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE,
      AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE, AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY,
      AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY, AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY,
      AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY, AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY,
      AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY, AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY,
      AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY, AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CAPABILITY,
      AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY, AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CAPABILITY,
      AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY};
typedef unsigned avdtp_media_codec_type_t;
typedef unsigned avdtp_media_type_t;
typedef struct { uint8_t seid, in_use, media_type, type; } avdtp_sep_t;
typedef struct {
    struct { unsigned media_type, media_codec_type, media_codec_information_len;
             uint8_t *media_codec_information; } media_codec;
} avdtp_capabilities_t;
typedef struct avdtp_stream_endpoint {
    uint8_t *media_codec_configuration_info;
    unsigned media_codec_configuration_len;
    uint16_t remote_configuration_bitmap;
    avdtp_capabilities_t remote_configuration;
} avdtp_stream_endpoint_t;
typedef struct {
    uint16_t sampling_frequency;
    uint8_t channel_mode, block_length, subbands, allocation_method, min_bitpool_value, max_bitpool_value;
} avdtp_configuration_sbc_t;
typedef struct {
    uint8_t object_type, channels, vbr;
    uint32_t sampling_frequency, bit_rate;
} avdtp_configuration_mpeg_aac_t;

static inline uint16_t store_bit16(uint16_t bits, unsigned bit, unsigned value) {
    return value ? bits | (1u<<bit) : bits & ~(1u<<bit);
}

avdtp_stream_endpoint_t *a2dp_source_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t codec,
        const uint8_t *capabilities, uint16_t capabilities_length, uint8_t *configuration, uint16_t configuration_length);
uint8_t a2dp_source_disconnect(uint16_t cid);
uint8_t avdtp_local_seid(const avdtp_stream_endpoint_t *endpoint);
void avdtp_source_register_delay_reporting_category(uint8_t seid);
void avdtp_set_preferred_sampling_frequency(avdtp_stream_endpoint_t *endpoint, uint32_t sampling_frequency);
void avdtp_set_preferred_channel_mode(avdtp_stream_endpoint_t *endpoint, uint8_t channel_mode);
uint16_t avdtp_choose_sbc_sampling_frequency(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap);
uint8_t avdtp_choose_sbc_channel_mode(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap);
uint8_t avdtp_choose_sbc_block_length(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap);
uint8_t avdtp_choose_sbc_subbands(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap);
uint8_t avdtp_choose_sbc_allocation_method(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap);
uint8_t avdtp_choose_sbc_max_bitpool_value(avdtp_stream_endpoint_t *endpoint, uint8_t value);
uint8_t avdtp_choose_sbc_min_bitpool_value(avdtp_stream_endpoint_t *endpoint, uint8_t value);
uint8_t avdtp_config_sbc_store(uint8_t *config, const avdtp_configuration_sbc_t *configuration);
uint8_t avdtp_config_mpeg_aac_store(uint8_t *config, const avdtp_configuration_mpeg_aac_t *configuration);
uint8_t avdtp_config_sbc_set_sampling_frequency(uint8_t *config, uint16_t hz);
uint8_t avdtp_config_mpeg_audio_set_sampling_frequency(uint8_t *config, uint16_t hz);
uint8_t avdtp_config_mpeg_aac_set_sampling_frequency(uint8_t *config, uint16_t hz);
uint8_t avdtp_config_atrac_set_sampling_frequency(uint8_t *config, uint16_t hz);
uint8_t avdtp_source_get_all_capabilities(uint16_t cid, uint8_t remote_seid);
uint8_t avdtp_source_set_configuration(uint16_t cid, uint8_t local_seid, uint8_t remote_seid,
                                       uint16_t services, avdtp_capabilities_t configuration);
uint8_t avdtp_source_reconfigure(uint16_t cid, uint8_t local_seid, uint8_t remote_seid,
                                 uint16_t services, avdtp_capabilities_t configuration);
// Event field getters.
uint8_t avdtp_subevent_signaling_sep_found_get_remote_seid(const uint8_t *event);
uint8_t avdtp_subevent_signaling_sep_found_get_in_use(const uint8_t *event);
uint8_t avdtp_subevent_signaling_sep_found_get_media_type(const uint8_t *event);
uint8_t avdtp_subevent_signaling_sep_found_get_sep_type(const uint8_t *event);
uint8_t avdtp_subevent_signaling_recovery_capability_get_recovery_type(const uint8_t *event);
uint8_t avdtp_subevent_signaling_recovery_capability_get_maximum_recovery_window_size(const uint8_t *event);
uint8_t avdtp_subevent_signaling_recovery_capability_get_maximum_number_media_packets(const uint8_t *event);
uint16_t avdtp_subevent_signaling_content_protection_capability_get_cp_type(const uint8_t *event);
uint16_t avdtp_subevent_signaling_content_protection_capability_get_cp_type_value_len(const uint8_t *event);
uint8_t avdtp_subevent_signaling_multiplexing_capability_get_fragmentation(const uint8_t *event);
uint8_t avdtp_subevent_signaling_multiplexing_capability_get_transport_identifiers_num(const uint8_t *event);
uint8_t avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_1(const uint8_t *event);
uint8_t avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_2(const uint8_t *event);
uint8_t avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_3(const uint8_t *event);
uint8_t avdtp_subevent_signaling_multiplexing_capability_get_tcid_1(const uint8_t *event);
uint8_t avdtp_subevent_signaling_multiplexing_capability_get_tcid_2(const uint8_t *event);
uint8_t avdtp_subevent_signaling_multiplexing_capability_get_tcid_3(const uint8_t *event);
uint8_t avdtp_subevent_signaling_delay_reporting_capability_get_remote_seid(const uint8_t *event);
uint8_t avdtp_subevent_signaling_header_compression_capability_get_back_ch(const uint8_t *event);
uint8_t avdtp_subevent_signaling_header_compression_capability_get_media(const uint8_t *event);
uint8_t avdtp_subevent_signaling_header_compression_capability_get_recovery(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_object_type_bitmap(const uint8_t *event);
uint16_t avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_sampling_frequency_bitmap(const uint8_t *event);
uint8_t avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_channels_bitmap(const uint8_t *event);
uint32_t avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_bit_rate(const uint8_t *event);
uint8_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_object_type_bitmap(const uint8_t *event);
uint16_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_sampling_frequency_bitmap(const uint8_t *event);
uint8_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_channels_bitmap(const uint8_t *event);
uint32_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_bit_rate(const uint8_t *event);
uint8_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_vbr(const uint8_t *event);
const uint8_t *avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information(const uint8_t *event);
uint16_t avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information_len(const uint8_t *event);
#endif
