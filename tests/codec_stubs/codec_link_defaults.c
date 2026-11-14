// SPDX-License-Identifier: GPL-3.0-only
// Weak no-op collaborators for linking src/btstack/a2dp_codec_configuration.c on the host.
// A test defines (strongly) every collaborator whose effect it observes.
#include <stddef.h>
#include "btstack.h"
#include "../../src/btstack/btstack_aacp.h"
#include "../../src/btstack/btstack_avdtp_source.h"
#include "../../src/btstack/media_session.h"
#define WEAK __attribute__((weak))

WEAK avdtp_stream_endpoint_t *a2dp_source_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t codec, const uint8_t *capabilities, uint16_t capabilities_length, uint8_t *configuration, uint16_t configuration_length) {(void)media_type;(void)codec;(void)capabilities;(void)capabilities_length;(void)configuration;(void)configuration_length;return NULL;}
WEAK uint8_t a2dp_source_disconnect(uint16_t cid) {(void)cid;return 0;}
WEAK uint8_t avdtp_local_seid(const avdtp_stream_endpoint_t *endpoint) {(void)endpoint;return 0;}
WEAK void avdtp_source_register_delay_reporting_category(uint8_t seid) {(void)seid;}
WEAK void avdtp_set_preferred_sampling_frequency(avdtp_stream_endpoint_t *endpoint, uint32_t sampling_frequency) {(void)endpoint;(void)sampling_frequency;}
WEAK void avdtp_set_preferred_channel_mode(avdtp_stream_endpoint_t *endpoint, uint8_t channel_mode) {(void)endpoint;(void)channel_mode;}
WEAK uint16_t avdtp_choose_sbc_sampling_frequency(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap) {(void)endpoint;(void)bitmap;return 0;}
WEAK uint8_t avdtp_choose_sbc_channel_mode(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap) {(void)endpoint;(void)bitmap;return 0;}
WEAK uint8_t avdtp_choose_sbc_block_length(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap) {(void)endpoint;(void)bitmap;return 0;}
WEAK uint8_t avdtp_choose_sbc_subbands(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap) {(void)endpoint;(void)bitmap;return 0;}
WEAK uint8_t avdtp_choose_sbc_allocation_method(avdtp_stream_endpoint_t *endpoint, uint8_t bitmap) {(void)endpoint;(void)bitmap;return 0;}
WEAK uint8_t avdtp_choose_sbc_max_bitpool_value(avdtp_stream_endpoint_t *endpoint, uint8_t value) {(void)endpoint;(void)value;return 0;}
WEAK uint8_t avdtp_choose_sbc_min_bitpool_value(avdtp_stream_endpoint_t *endpoint, uint8_t value) {(void)endpoint;(void)value;return 0;}
WEAK uint8_t avdtp_config_sbc_store(uint8_t *config, const avdtp_configuration_sbc_t *configuration) {(void)config;(void)configuration;return 0;}
WEAK uint8_t avdtp_config_mpeg_aac_store(uint8_t *config, const avdtp_configuration_mpeg_aac_t *configuration) {(void)config;(void)configuration;return 0;}
WEAK uint8_t avdtp_config_sbc_set_sampling_frequency(uint8_t *config, uint16_t hz) {(void)config;(void)hz;return 0;}
WEAK uint8_t avdtp_config_mpeg_audio_set_sampling_frequency(uint8_t *config, uint16_t hz) {(void)config;(void)hz;return 0;}
WEAK uint8_t avdtp_config_mpeg_aac_set_sampling_frequency(uint8_t *config, uint16_t hz) {(void)config;(void)hz;return 0;}
WEAK uint8_t avdtp_config_atrac_set_sampling_frequency(uint8_t *config, uint16_t hz) {(void)config;(void)hz;return 0;}
WEAK uint8_t avdtp_source_get_all_capabilities(uint16_t cid, uint8_t remote_seid) {(void)cid;(void)remote_seid;return 0;}
WEAK uint8_t avdtp_source_set_configuration(uint16_t cid, uint8_t local_seid, uint8_t remote_seid, uint16_t services, avdtp_capabilities_t configuration) {(void)cid;(void)local_seid;(void)remote_seid;(void)services;(void)configuration;return 0;}
WEAK uint8_t avdtp_source_reconfigure(uint16_t cid, uint8_t local_seid, uint8_t remote_seid, uint16_t services, avdtp_capabilities_t configuration) {(void)cid;(void)local_seid;(void)remote_seid;(void)services;(void)configuration;return 0;}
WEAK uint8_t avdtp_subevent_signaling_sep_found_get_remote_seid(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_sep_found_get_in_use(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_sep_found_get_media_type(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_sep_found_get_sep_type(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_recovery_capability_get_recovery_type(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_recovery_capability_get_maximum_recovery_window_size(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_recovery_capability_get_maximum_number_media_packets(const uint8_t *event) {(void)event;return 0;}
WEAK uint16_t avdtp_subevent_signaling_content_protection_capability_get_cp_type(const uint8_t *event) {(void)event;return 0;}
WEAK uint16_t avdtp_subevent_signaling_content_protection_capability_get_cp_type_value_len(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_multiplexing_capability_get_fragmentation(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_multiplexing_capability_get_transport_identifiers_num(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_1(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_2(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_3(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_multiplexing_capability_get_tcid_1(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_multiplexing_capability_get_tcid_2(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_multiplexing_capability_get_tcid_3(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_delay_reporting_capability_get_remote_seid(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_header_compression_capability_get_back_ch(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_header_compression_capability_get_media(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_header_compression_capability_get_recovery(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_object_type_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint16_t avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_sampling_frequency_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_channels_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint32_t avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_bit_rate(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_object_type_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint16_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_sampling_frequency_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_channels_bitmap(const uint8_t *event) {(void)event;return 0;}
WEAK uint32_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_bit_rate(const uint8_t *event) {(void)event;return 0;}
WEAK uint8_t a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_vbr(const uint8_t *event) {(void)event;return 0;}
WEAK const uint8_t *avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information(const uint8_t *event) {(void)event;return NULL;}
WEAK uint16_t avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information_len(const uint8_t *event) {(void)event;return 0;}

WEAK bool get_a2dp_connected_flag(void) {return false;}
WEAK void aacp_disconnect(void) {}
WEAK bool aacp_model_game_supported(void) {return false;}
WEAK void set_next_capablity_and_start_stream(void) {}
WEAK uint16_t media_session_cid(void) {return 0;}
WEAK const uint8_t *media_session_active_device(void) {static const uint8_t none[6];return none;}
WEAK uint8_t media_session_codec(void) {return 0;}
WEAK playback_options_t media_session_playback_options(void) {return (playback_options_t){0};}
WEAK bool media_session_codec_scan_finished(void) {return false;}
WEAK struct avdtp_stream_endpoint *media_session_local_endpoint(void) {return NULL;}
WEAK void media_session_set_local_endpoint(struct avdtp_stream_endpoint *endpoint) {(void)endpoint;}
WEAK uint8_t media_session_local_seid(void) {return 0;}
WEAK void media_session_set_local_seid(uint8_t local_seid) {(void)local_seid;}
WEAK uint8_t media_session_remote_seid(void) {return 0;}
WEAK void media_session_set_remote_seid(uint8_t remote_seid) {(void)remote_seid;}
