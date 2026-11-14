/*
 * A2DP codec configuration for the selected AirPods: local SBC, AAC and Apple
 * AAC-ELD stream endpoints and their capabilities, the catalog of remote stream
 * endpoints discovered on the current media session, and choosing and sending
 * the codec configuration. Split out of btstack_avdtp_source.c; main/Bluetooth
 * context only. The encoders themselves stay with the media pipeline.
 */
/*
 * Copyright (C) 2016 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "btstack.h"
#include "btstack_avdtp_source.h"
#include "btstack_aacp.h"
#include "playback_eld.h"
#include "playback_options.h"
#include "a2dp_codec_identifiers.h"
#include "media_session.h"
#include "host_test_visibility.h"
#include "a2dp_codec_configuration.h"
#include "a2dp_codec_configuration_internal.h"

// Matches btstack_avdtp_source.c, which always builds the FDK AAC encoder.
#define HAVE_AAC_FDK

// on pico 2w the max stable aac bit rate under 512 simples without vbr is around 220000
static int max_aac_bit_rate = 220000;

typedef struct {
    // bitmaps
    uint8_t sampling_frequency_bitmap;
    uint8_t channel_mode_bitmap;
    uint8_t block_length_bitmap;
    uint8_t subbands_bitmap;
    uint8_t allocation_method_bitmap;
    uint8_t min_bitpool_value;
    uint8_t max_bitpool_value;
} media_codec_information_sbc_t;

PRIVATE_UNLESS_HOST_TEST uint16_t     num_remote_seps;
PRIVATE_UNLESS_HOST_TEST uint16_t     cur_num_remote_seps;
PRIVATE_UNLESS_HOST_TEST bool        remote_catalog_truncated;

PRIVATE_UNLESS_HOST_TEST uint16_t                  media_codec_config_len;
PRIVATE_UNLESS_HOST_TEST uint8_t                   media_codec_config_data[AVDTP_MAX_MEDIA_CODEC_CONFIG_LEN];

PRIVATE_UNLESS_HOST_TEST a2dp_remote_endpoint_t remote_seps[AVDTP_MAX_SEP_NUM];
PRIVATE_UNLESS_HOST_TEST uint8_t selected_remote_sep_index;

PRIVATE_UNLESS_HOST_TEST bool have_aaceld_codec_capabilities;
// Set once an AAC configuration was offered; an AAC configuration event is accepted only then.
PRIVATE_UNLESS_HOST_TEST bool finish_setup_aac;

// setup stream endpoints

avdtp_stream_endpoint_t * stream_endpoint_sbc;
avdtp_stream_endpoint_t * stream_endpoint_aac;
avdtp_stream_endpoint_t * stream_endpoint_aaceld;

static const uint8_t media_sbc_codec_capabilities[] = {
    0x17, // 48 kHz; stereo, joint stereo and dual channel.
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
};


#ifdef HAVE_AAC_FDK
static uint8_t media_aac_codec_capabilities[] = {
        0xC0, // MPEG-2/4 AAC-LC
        0x00,
        0x84, // 48 kHz stereo
        0x03, 0x5B, 0x60 // CBR, 220 kbps ceiling
};

typedef struct {
    int reconfigure;
    int sampling_frequency_bitmap;
    int object_type_bitmap;
    int bit_rate;
    int channels_bitmap;
    int vbr;
} avdtp_media_codec_capabilities_aac_t;

#endif

// Apple AAC-ELD vendor codec capabilities (vendor_id=0x004C, codec_id=0x8001)
// Format: vendor_id(4 LE) + codec_id(2 LE) + obj_type(2) + freq+ch(2) + rsvd(1) + vbr+bitrate(3)
PRIVATE_UNLESS_HOST_TEST uint8_t media_aaceld_codec_capabilities[] = {
        0x4C, 0x00, 0x00, 0x00,   // vendor_id = 0x004C (Apple)
        0x01, 0x80,                // codec_id  = 0x8001
        0x00, 0x80,                // object_type = 0x0080 (AAC-ELD)
        0x00, 0x84,                // freq bitmap 0x008 (48 kHz) + stereo
        0x00,                      // reserved
        0x83, 0xE8, 0x00           // VBR=1, bitrate=256000
};

// configurations for local stream endpoints
static uint8_t local_stream_endpoint_sbc_media_codec_configuration[4];
PRIVATE_UNLESS_HOST_TEST uint8_t local_stream_endpoint_aaceld_media_codec_configuration[14];

#ifdef HAVE_AAC_FDK
static uint8_t local_stream_endpoint_aac_media_codec_configuration[6];
#endif

PRIVATE_UNLESS_HOST_TEST int find_remote_seid(uint8_t remote_seid){
    int i;
    for (i=0;i<num_remote_seps;i++){
        if (remote_seps[i].seid == remote_seid){
            return i;
        }
    }
    return -1;
}

static void dump_sbc_capability(media_codec_information_sbc_t media_codec_sbc){
    printf("    - sampling_frequency: 0x%02x\n", media_codec_sbc.sampling_frequency_bitmap);
    printf("    - channel_mode: 0x%02x\n", media_codec_sbc.channel_mode_bitmap);
    printf("    - block_length: 0x%02x\n", media_codec_sbc.block_length_bitmap);
    printf("    - subbands: 0x%02x\n", media_codec_sbc.subbands_bitmap);
    printf("    - allocation_method: 0x%02x\n", media_codec_sbc.allocation_method_bitmap);
    printf("    - bitpool_value [%d, %d] \n", media_codec_sbc.min_bitpool_value, media_codec_sbc.max_bitpool_value);
}

void a2dp_codec_configuration_print_remote_endpoints(void){
    printf("Remote Endpoints:\n");
    int i;
    for (i=0;i<num_remote_seps;i++) {
        printf("- %u. remote seid %u\n", i, remote_seps[i].seid);
    }
}

static const char * codec_name_for_type(avdtp_media_codec_type_t codec_type){
    switch (codec_type){
        case AVDTP_CODEC_SBC:            return "SBC";
        case AVDTP_CODEC_MPEG_1_2_AUDIO: return "MPEG_1_2_AUDIO";
        case AVDTP_CODEC_MPEG_2_4_AAC:   return "MPEG_2_4_AAC";
        case AVDTP_CODEC_ATRAC_FAMILY:   return "ATRAC_FAMILY";
        case AVDTP_CODEC_NON_A2DP:       return "NON_A2DP/Vendor";
        default:                         return "Unknown";
    }
}

// An incoming/reconnect config can precede our asynchronous peer capability discovery.
bool playback_eld_peer_configuration(uint8_t local_seid, uint8_t remote_seid,
                                             const uint8_t *config, uint16_t size) {
    if (!local_seid || local_seid > 0x3e || !remote_seid || remote_seid > 0x3e ||
        !stream_endpoint_aaceld || local_seid != avdtp_local_seid(stream_endpoint_aaceld) ||
        !playback_eld_configuration_supported(config, size,
            media_aaceld_codec_capabilities, sizeof(media_aaceld_codec_capabilities))) return false;
    for (int i = 0; i < num_remote_seps; ++i) {
        if (remote_seps[i].seid != remote_seid) continue;
        if (!remote_seps[i].have_media_codec_capabilities) return true;
        if (remote_seps[i].codec_type != AVDTP_CODEC_NON_A2DP ||
            remote_seps[i].vendor_id != A2DP_CODEC_VENDOR_ID_APPLE ||
            remote_seps[i].codec_id != A2DP_APPLE_CODEC_AAC_ELD) return false;
        const uint8_t *event = remote_seps[i].media_codec_event;
        return playback_eld_configuration_supported(config, size,
            avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information(event),
            avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information_len(event));
    }
    return true;
}

void playback_eld_bind_configuration(uint8_t local_seid, uint8_t remote_seid) {
    media_session_set_local_endpoint(stream_endpoint_aaceld);
    media_session_set_local_seid(local_seid);
    media_session_set_remote_seid(remote_seid);
    int index = find_remote_seid(remote_seid);
    selected_remote_sep_index = index >= 0 ? index : AVDTP_MAX_SEP_NUM;
}

int setup_sbc_configuration(){

    int sbc_num = -1;
    for (int i = 0; i < num_remote_seps; i++){
        if (remote_seps[i].have_media_codec_capabilities &&
            remote_seps[i].codec_type == AVDTP_CODEC_SBC){
            printf("found sbc!!! Remote Stream Endpoints ID is %d\n", i);
            selected_remote_sep_index = i;
            sbc_num = i;
            break;
        }
    }

    if (sbc_num < 0) return -1;

    // SBC endpoint persists across connections.
    if (stream_endpoint_sbc == NULL) {
        stream_endpoint_sbc = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, (uint8_t *) media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), (uint8_t*) local_stream_endpoint_sbc_media_codec_configuration, sizeof(local_stream_endpoint_sbc_media_codec_configuration));
        btstack_assert(stream_endpoint_sbc != NULL);
        stream_endpoint_sbc->media_codec_configuration_info = local_stream_endpoint_sbc_media_codec_configuration;
        stream_endpoint_sbc->media_codec_configuration_len  = sizeof(local_stream_endpoint_sbc_media_codec_configuration);
    }
    avdtp_source_register_delay_reporting_category(avdtp_local_seid(stream_endpoint_sbc));
    avdtp_set_preferred_sampling_frequency(stream_endpoint_sbc, 48000);
    avdtp_set_preferred_channel_mode(stream_endpoint_sbc, AVDTP_SBC_STEREO);

    // set up local stream_endpoint; need change
    media_session_set_local_endpoint(stream_endpoint_sbc);

    // store local seid
    media_session_set_local_seid(avdtp_local_seid(media_session_local_endpoint()));
    media_session_set_remote_seid(remote_seps[sbc_num].seid);


    // choose SBC config params
    const uint8_t * packet = remote_seps[sbc_num].media_codec_event;

    // choose SBC config params
    avdtp_configuration_sbc_t configuration;
    configuration.sampling_frequency = avdtp_choose_sbc_sampling_frequency(media_session_local_endpoint(), avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet));
    configuration.channel_mode       = avdtp_choose_sbc_channel_mode(media_session_local_endpoint(), avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet));
    configuration.block_length       = avdtp_choose_sbc_block_length(media_session_local_endpoint(), avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet));
    configuration.subbands           = avdtp_choose_sbc_subbands(media_session_local_endpoint(), avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet));
    configuration.allocation_method  = avdtp_choose_sbc_allocation_method(media_session_local_endpoint(), avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet));
    configuration.max_bitpool_value  = avdtp_choose_sbc_max_bitpool_value(media_session_local_endpoint(), avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet));
    configuration.min_bitpool_value  = avdtp_choose_sbc_min_bitpool_value(media_session_local_endpoint(), avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet));

    if (configuration.sampling_frequency != 48000 ||
        configuration.channel_mode == AVDTP_CHANNEL_MODE_MONO) return -1;
    avdtp_config_sbc_store(media_codec_config_data, &configuration);
    media_codec_config_len = 4;

    avdtp_capabilities_t new_configuration;
    new_configuration.media_codec.media_type = AVDTP_AUDIO;
    new_configuration.media_codec.media_codec_type = remote_seps[selected_remote_sep_index].codec_type ;
    new_configuration.media_codec.media_codec_information_len = media_codec_config_len;
    new_configuration.media_codec.media_codec_information = media_codec_config_data;
    int status = avdtp_source_set_configuration(media_session_cid(), media_session_local_seid(), media_session_remote_seid(), 1 << AVDTP_MEDIA_CODEC, new_configuration);
    return status;
}


PRIVATE_UNLESS_HOST_TEST int setup_aac_configuration(){

    #ifdef HAVE_AAC_FDK
    // - AAC
    if (stream_endpoint_aac == NULL) {
        stream_endpoint_aac = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_MPEG_2_4_AAC, (uint8_t *) media_aac_codec_capabilities, sizeof(media_aac_codec_capabilities), (uint8_t*) local_stream_endpoint_aac_media_codec_configuration, sizeof(local_stream_endpoint_aac_media_codec_configuration));
        btstack_assert(stream_endpoint_aac != NULL);
        stream_endpoint_aac->media_codec_configuration_info = local_stream_endpoint_aac_media_codec_configuration;
        stream_endpoint_aac->media_codec_configuration_len  = sizeof(local_stream_endpoint_aac_media_codec_configuration);
        avdtp_source_register_delay_reporting_category(avdtp_local_seid(stream_endpoint_aac));
    }
    #endif

    int aac_num = -1;
    for (int i = 0; i < num_remote_seps; i++){
        if (remote_seps[i].have_media_codec_capabilities &&
            remote_seps[i].codec_type == AVDTP_CODEC_MPEG_2_4_AAC){
            printf("found aac!!! Remote Stream Endpoints ID is %d\n", i);
            selected_remote_sep_index = i;
            aac_num = i;
            break;
        }
    }

    if (aac_num < 0) return -1;
    media_session_set_local_endpoint(stream_endpoint_aac);

    // store local seid
    media_session_set_local_seid(avdtp_local_seid(media_session_local_endpoint()));
    media_session_set_remote_seid(remote_seps[aac_num].seid);

    const uint8_t *cap = remote_seps[aac_num].media_codec_event;
    uint8_t object_bitmap = avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_object_type_bitmap(cap);
    uint16_t rate_bitmap = avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_sampling_frequency_bitmap(cap);
    uint8_t channels_bitmap = avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_channels_bitmap(cap);
    if (!(rate_bitmap & 8) || !(channels_bitmap & 4) || !(object_bitmap & 0x60)) return -1;
    avdtp_configuration_mpeg_aac_t configuration = {0};
    configuration.object_type = (object_bitmap & 0x20) ? AVDTP_AAC_MPEG4_LC : AVDTP_AAC_MPEG2_LC;
    configuration.sampling_frequency = 48000;
    configuration.channels = 2;
    uint32_t remote_bitrate = avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_bit_rate(cap);
    uint32_t desired=media_session_playback_options().bitrate ? media_session_playback_options().bitrate : (uint32_t)max_aac_bit_rate;
    if (media_session_playback_options().bitrate && remote_bitrate && desired>remote_bitrate) return -1;
    configuration.bit_rate = remote_bitrate == 0 || remote_bitrate > desired ? desired : remote_bitrate;
    if (configuration.bit_rate < 96000) return -1;
    configuration.vbr = 0;
    if (avdtp_config_mpeg_aac_store(media_codec_config_data, &configuration) != ERROR_CODE_SUCCESS) return -1;
    media_codec_config_len = 6;

    avdtp_capabilities_t new_configuration;
    new_configuration.media_codec.media_type = AVDTP_AUDIO;
    new_configuration.media_codec.media_codec_type =  2,    // Media Codec Type: A2DP_MEDIA_CT_AAC
    new_configuration.media_codec.media_codec_information_len = media_codec_config_len;
    new_configuration.media_codec.media_codec_information = media_codec_config_data;
    int status = avdtp_source_set_configuration(media_session_cid(), media_session_local_seid(), media_session_remote_seid(), 1 << AVDTP_MEDIA_CODEC, new_configuration);
    printf("Set AAC Connection Result is %d\n", status);
    finish_setup_aac = true;

    return status;
}



PRIVATE_UNLESS_HOST_TEST int setup_aaceld_configuration(){
    int aaceld_num = -1;
    if (num_remote_seps == 0){
        printf("Remote Stream Endpoints not discovered yet\n");
        return -1;
    }
    for (int i = 0; i < num_remote_seps; i++){
        if (remote_seps[i].vendor_id == A2DP_CODEC_VENDOR_ID_APPLE && remote_seps[i].codec_id == A2DP_APPLE_CODEC_AAC_ELD){
            printf("found Apple AAC-ELD!!! Remote Stream Endpoints ID is %d\n", i);
            selected_remote_sep_index = i;
            aaceld_num = i;
            break;
        }
    }

    if (aaceld_num < 0){
        printf("not found Apple AAC-ELD!!!\n");
        return -1;
    }

    avdtp_media_codec_type_t codec_type = remote_seps[aaceld_num].codec_type;
    if (codec_type != AVDTP_CODEC_NON_A2DP) {
        printf("AAC-ELD codec type mismatch!!!\n");
        return -1;
    }

    // Create AAC-ELD stream endpoint (only once)
    if (stream_endpoint_aaceld == NULL) {
        stream_endpoint_aaceld = a2dp_source_create_stream_endpoint(
            AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP,
            (uint8_t *) media_aaceld_codec_capabilities, sizeof(media_aaceld_codec_capabilities),
            (uint8_t*) local_stream_endpoint_aaceld_media_codec_configuration,
            sizeof(local_stream_endpoint_aaceld_media_codec_configuration));
        btstack_assert(stream_endpoint_aaceld != NULL);
        stream_endpoint_aaceld->media_codec_configuration_info = local_stream_endpoint_aaceld_media_codec_configuration;
        stream_endpoint_aaceld->media_codec_configuration_len  = sizeof(local_stream_endpoint_aaceld_media_codec_configuration);
        avdtp_source_register_delay_reporting_category(avdtp_local_seid(stream_endpoint_aaceld));
    }

    media_session_set_local_endpoint(stream_endpoint_aaceld);

    media_session_set_local_seid(avdtp_local_seid(media_session_local_endpoint()));
    media_session_set_remote_seid(remote_seps[aaceld_num].seid);

    media_session_local_endpoint()->remote_configuration_bitmap = store_bit16(media_session_local_endpoint()->remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
    media_session_local_endpoint()->remote_configuration.media_codec.media_type = AVDTP_AUDIO;
    media_session_local_endpoint()->remote_configuration.media_codec.media_codec_type = codec_type;

    const uint8_t *event = remote_seps[aaceld_num].media_codec_event;
    const uint8_t *cap = avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information(event);
    uint16_t cap_len = avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information_len(event);
    if (!playback_options_eld(cap, cap_len, media_session_playback_options(),media_codec_config_data)) return -1;
    media_codec_config_len = PLAYBACK_ELD_CONFIG_SIZE;

    avdtp_capabilities_t new_configuration = {0};
    new_configuration.media_codec.media_type = AVDTP_AUDIO;
    new_configuration.media_codec.media_codec_type = codec_type;
    new_configuration.media_codec.media_codec_information_len = media_codec_config_len;
    new_configuration.media_codec.media_codec_information = media_codec_config_data;
    uint16_t services = 1 << AVDTP_MEDIA_CODEC;
    if (remote_seps[aaceld_num].delay_reporting) services |= 1 << AVDTP_DELAY_REPORTING;
    int status = avdtp_source_set_configuration(media_session_cid(), media_session_local_seid(), media_session_remote_seid(), services, new_configuration);

    printf("Set Apple AAC-ELD Connection Result is %d\n", status);
    return status;
}

int set_next_codec(uint8_t num){
    if (media_session_playback_options().codec==1) return setup_sbc_configuration();
    if (media_session_playback_options().codec==2) return setup_aac_configuration();
    if (media_session_playback_options().codec==3) return setup_aaceld_configuration();
    //return 0;
    //return setup_sbc_configuration();
    switch (num){
        case 0: // AAC-ELD > AAC > SBC
            if (have_aaceld_codec_capabilities){
                return setup_aaceld_configuration();
            } else {
                // Check if remote actually has AAC before trying
                bool have_aac = false;
                for (int i = 0; i < num_remote_seps; i++){
                    if (remote_seps[i].codec_type == AVDTP_CODEC_MPEG_2_4_AAC){
                        have_aac = true;
                        break;
                    }
                }
                if (have_aac) {
                    return setup_aac_configuration();
                } else {
                    printf("No AAC support, falling back to SBC\n");
                    return setup_sbc_configuration();
                }
            }
        case 1: // sbc
            return setup_sbc_configuration();
        default:
            return 1;
    }
}

bool playback_game_eligible(void) {
    int index = find_remote_seid(media_session_remote_seid());
    return get_a2dp_connected_flag() && media_session_codec()==4 && aacp_model_game_supported() &&
        index >= 0 && index < num_remote_seps && remote_seps[index].delay_reporting &&
        remote_seps[index].have_media_codec_capabilities && remote_seps[index].codec_type == AVDTP_CODEC_NON_A2DP &&
        remote_seps[index].vendor_id == A2DP_CODEC_VENDOR_ID_APPLE && remote_seps[index].codec_id == A2DP_APPLE_CODEC_AAC_ELD &&
        media_session_local_endpoint() &&
        (media_session_local_endpoint()->remote_configuration_bitmap & (1 << AVDTP_DELAY_REPORTING));
}

bool playback_profile_preflight(playback_options_t next, const uint8_t peer[6]) {
    bool complete=get_a2dp_connected_flag() && !memcmp(peer,media_session_active_device(),6) &&
        media_session_codec_scan_finished() && !remote_catalog_truncated && num_remote_seps &&
        cur_num_remote_seps==num_remote_seps;
    uint8_t matching=0;
    for (unsigned i=0;i<num_remote_seps && complete;i++) {
        if (!remote_seps[i].have_media_codec_capabilities) { complete=false; break; }
        const uint8_t *event=remote_seps[i].media_codec_event;
        if (remote_seps[i].codec_type==AVDTP_CODEC_SBC) matching|=1u<<1;
        else if (remote_seps[i].codec_type==AVDTP_CODEC_MPEG_2_4_AAC && next.codec==2) {
            if (playback_options_aac_supported(next,
                avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_object_type_bitmap(event),
                avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_sampling_frequency_bitmap(event),
                avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_channels_bitmap(event),
                avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_bit_rate(event))) matching|=1u<<2;
        } else if (remote_seps[i].codec_type==AVDTP_CODEC_NON_A2DP && next.codec==3 &&
                   remote_seps[i].vendor_id==A2DP_CODEC_VENDOR_ID_APPLE &&
                   remote_seps[i].codec_id==A2DP_APPLE_CODEC_AAC_ELD) {
            uint8_t negotiated[PLAYBACK_ELD_CONFIG_SIZE];
            if (playback_options_eld(
                avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information(event),
                avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information_len(event),
                next,negotiated)) matching|=1u<<3;
        }
    }
    return playback_options_preflight(next,complete,matching);
}

// Stream endpoint discovery and remote capability events of the current media
// session; the caller has already matched the AVDTP signaling channel.
bool a2dp_codec_configuration_discovery_event(uint8_t *packet, uint16_t size) {
    avdtp_sep_t sep;
    uint8_t remote_seid;
    int local_remote_seid_index;
    uint32_t vendor_id;
    uint16_t codec_id;

    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:
            memset(&sep, 0, sizeof(avdtp_sep_t));
            sep.seid = avdtp_subevent_signaling_sep_found_get_remote_seid(packet);;
            sep.in_use = avdtp_subevent_signaling_sep_found_get_in_use(packet);
            sep.media_type = avdtp_subevent_signaling_sep_found_get_media_type(packet);
            sep.type = avdtp_subevent_signaling_sep_found_get_sep_type(packet);
            printf("Found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", sep.seid, sep.in_use, sep.media_type, sep.type);
            if (sep.type == AVDTP_SINK && sep.media_type == AVDTP_AUDIO) {
                if (num_remote_seps < AVDTP_MAX_SEP_NUM) {
                    remote_seps[num_remote_seps].seid = sep.seid;
                    num_remote_seps++;
                } else remote_catalog_truncated=true;
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE:
            if (num_remote_seps == 0) {
                aacp_disconnect();
                a2dp_source_disconnect(media_session_cid());
                break;
            }
            // select remote if there's only a single remote
            if (num_remote_seps == 1){
                media_session_set_remote_seid(remote_seps[0].seid);
                printf("Only one remote Stream Endpoint with SEID %u, select it for initiator commands\n", media_session_remote_seid());
            }


            cur_num_remote_seps = 0;
            // find 1st capabilities; should be sbc
            avdtp_source_get_all_capabilities(media_session_cid(), remote_seps[0].seid);

            break;

        case AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE:
            cur_num_remote_seps ++;
            if (cur_num_remote_seps < num_remote_seps){
                printf("\n\n Getting next CAPABILITY \n\n");
                avdtp_source_get_all_capabilities(media_session_cid(), remote_seps[cur_num_remote_seps].seid);
            }else{
                printf("finish scaning CAPABILITIES codecs\n");
                set_next_capablity_and_start_stream();
            }
            break;


        case AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY:
            printf("CAPABILITY - MEDIA_TRANSPORT supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY:
            printf("CAPABILITY - REPORTING supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY:
            printf("CAPABILITY - RECOVERY supported on remote: \n");
            printf("    - recovery_type                %d\n", avdtp_subevent_signaling_recovery_capability_get_recovery_type(packet));
            printf("    - maximum_recovery_window_size %d\n", avdtp_subevent_signaling_recovery_capability_get_maximum_recovery_window_size(packet));
            printf("    - maximum_number_media_packets %d\n", avdtp_subevent_signaling_recovery_capability_get_maximum_number_media_packets(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY:
            if (size < 10 || avdtp_subevent_signaling_content_protection_capability_get_cp_type_value_len(packet) > size - 10u) break;
            printf("CAPABILITY - CONTENT_PROTECTION supported on remote: \n");
            printf("    - cp_type           %d\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type(packet));
            printf("    - cp_type_value_len %d\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type_value_len(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY:
            printf("CAPABILITY - MULTIPLEXING supported on remote: \n");
            printf("    - fragmentation                  %d\n", avdtp_subevent_signaling_multiplexing_capability_get_fragmentation(packet));
            printf("    - transport_identifiers_num      %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_identifiers_num(packet));
            printf("    - transport_session_identifier_1 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_1(packet));
            printf("    - transport_session_identifier_2 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_2(packet));
            printf("    - transport_session_identifier_3 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_3(packet));
            printf("    - tcid_1                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_1(packet));
            printf("    - tcid_2                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_2(packet));
            printf("    - tcid_3                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_3(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY:
            if (size<6) break;
            local_remote_seid_index=find_remote_seid(avdtp_subevent_signaling_delay_reporting_capability_get_remote_seid(packet));
            if (local_remote_seid_index>=0) remote_seps[local_remote_seid_index].delay_reporting=true;
            printf("CAPABILITY - DELAY_REPORTING supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY:
            printf("CAPABILITY - HEADER_COMPRESSION supported on remote: \n");
            printf("    - back_ch   %d\n", avdtp_subevent_signaling_header_compression_capability_get_back_ch(packet));
            printf("    - media     %d\n", avdtp_subevent_signaling_header_compression_capability_get_media(packet));
            printf("    - recovery  %d\n", avdtp_subevent_signaling_header_compression_capability_get_recovery(packet));
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:{
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            if (local_remote_seid_index < 0 || size > AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN) break;
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].codec_type = AVDTP_CODEC_SBC;
            remote_seps[local_remote_seid_index].have_media_codec_capabilities = true;

            printf("CAPABILITY - MEDIA_CODEC: SBC, remote seid %u: \n", remote_seid);

            media_codec_information_sbc_t   sbc_capability;
            sbc_capability.sampling_frequency_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet);
            sbc_capability.channel_mode_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet);
            sbc_capability.block_length_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet);
            sbc_capability.subbands_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet);
            sbc_capability.allocation_method_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet);
            sbc_capability.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet);
            sbc_capability.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet);
            dump_sbc_capability(sbc_capability);

            break;
        }

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CAPABILITY:
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            if (local_remote_seid_index < 0 || size > AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN) break;
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].codec_type = AVDTP_CODEC_MPEG_1_2_AUDIO;
            remote_seps[local_remote_seid_index].have_media_codec_capabilities = true;
            printf("CAPABILITY - MEDIA_CODEC: MPEG AUDIO, remote seid %u: \n", remote_seid);

            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY: {
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            if (local_remote_seid_index < 0 || size > AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN) break;
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].codec_type = AVDTP_CODEC_MPEG_2_4_AAC;
            remote_seps[local_remote_seid_index].have_media_codec_capabilities = true;
            printf("CAPABILITY - MEDIA_CODEC: MPEG AAC, remote seid %u: \n", remote_seid);

            avdtp_media_codec_capabilities_aac_t aac_capabilities;
            aac_capabilities.sampling_frequency_bitmap = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_sampling_frequency_bitmap(packet);
            aac_capabilities.object_type_bitmap = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_object_type_bitmap(packet);
            aac_capabilities.channels_bitmap = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_channels_bitmap(packet);
            aac_capabilities.bit_rate = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_bit_rate(packet);
            aac_capabilities.vbr = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_vbr(packet);
            printf("A2DP Source: Received AAC capabilities! Sampling frequency bitmap: 0x%04x, object type %u, channel mode %u, bitrate %u, vbr: %u\n",
                   aac_capabilities.sampling_frequency_bitmap, aac_capabilities.object_type_bitmap, aac_capabilities.channels_bitmap,
                   aac_capabilities.bit_rate, aac_capabilities.vbr);
            break;
        }

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CAPABILITY:
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            if (local_remote_seid_index < 0 || size > AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN) break;
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].codec_type = AVDTP_CODEC_ATRAC_FAMILY;
            remote_seps[local_remote_seid_index].have_media_codec_capabilities = true;
            printf("CAPABILITY - MEDIA_CODEC: ATRAC, remote seid %u: \n", remote_seid);

            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:
            if (size < 17) break;
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            if (local_remote_seid_index < 0 || size > AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN) break;
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].codec_type  = AVDTP_CODEC_NON_A2DP;
            remote_seps[local_remote_seid_index].have_media_codec_capabilities = true;
            const uint8_t *media_info = avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information(packet);
            uint16_t info_len = avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information_len(packet);
            if (info_len < 6 || media_info + info_len > packet + size) break;
            vendor_id = get_vendor_id(media_info);
            codec_id = get_codec_id(media_info);

            remote_seps[local_remote_seid_index].vendor_id = vendor_id;
            remote_seps[local_remote_seid_index].codec_id = codec_id;

            if (vendor_id == A2DP_CODEC_VENDOR_ID_APPLE && codec_id == A2DP_APPLE_CODEC_AAC_ELD)
            {
                have_aaceld_codec_capabilities = playback_eld_capability(media_info, info_len);
                printf("CAPABILITY - Apple AAC-ELD, remote seid %u\n", remote_seid);
                printf("  raw bytes (%u): ", info_len);
                for (int k = 0; k < info_len; k++) {
                    printf("%02x ", media_info[k]);
                }
                printf("\n");
            }
            else {
                printf("CAPABILITY - MEDIA_CODEC: OTHER, remote seid %u: \n", remote_seid);
                printf("vendor_id 0x%04" PRIx32 ": \n", vendor_id);
                printf("codec_id: 0x%04x: \n", codec_id);
            }

            break;

        default:
            return false;
    }
    return true;
}

void a2dp_codec_configuration_reset(void) {
    memset(remote_seps, 0, sizeof(remote_seps));
    cur_num_remote_seps=0;
    remote_catalog_truncated=false;
    num_remote_seps = 0;
    selected_remote_sep_index = 0;
    have_aaceld_codec_capabilities = false;
    finish_setup_aac = false;
}

void a2dp_codec_configuration_released(void) {
    have_aaceld_codec_capabilities = false;
    finish_setup_aac = false;
}

bool a2dp_codec_configuration_aac_offered(void) {
    return finish_setup_aac;
}

uint16_t a2dp_codec_configuration_remote_endpoint_count(void) {
    return num_remote_seps;
}

void a2dp_codec_configuration_select_remote_endpoint(uint8_t index) {
    selected_remote_sep_index = index;
    media_session_set_remote_seid(remote_seps[selected_remote_sep_index].seid);
}

uint8_t a2dp_codec_configuration_reconfigure_selected_endpoint(void) {
    if (num_remote_seps == 0){
        printf("Remote Stream Endpoints not discovered yet, please discover stream endpoints first\n");
        return ERROR_CODE_SUCCESS;
    }
    if (selected_remote_sep_index >= num_remote_seps ||
        remote_seps[selected_remote_sep_index].have_media_codec_capabilities == false){
        printf("Remote Stream Endpoints Media Codec Capabilities not received yet, please get (all) capabilities for stream endpoint with seid %u first\n",  media_session_remote_seid());
        return ERROR_CODE_SUCCESS;
    }
    printf("Reconfigure stream endpoint with seid %d\n", media_session_remote_seid());
    avdtp_media_codec_type_t media_codec_type = remote_seps[selected_remote_sep_index].codec_type;
    avdtp_capabilities_t new_configuration;
    new_configuration.media_codec.media_type = AVDTP_AUDIO;

    const uint16_t new_sampling_frequency = 48000;

    switch (media_codec_type){
        case AVDTP_CODEC_SBC:
            avdtp_config_sbc_set_sampling_frequency(media_codec_config_data, new_sampling_frequency);
            break;
        case AVDTP_CODEC_MPEG_1_2_AUDIO:
            avdtp_config_mpeg_audio_set_sampling_frequency(media_codec_config_data, new_sampling_frequency);
            break;
        case AVDTP_CODEC_MPEG_2_4_AAC:
            avdtp_config_mpeg_aac_set_sampling_frequency(media_codec_config_data, new_sampling_frequency);
            break;
        case AVDTP_CODEC_ATRAC_FAMILY:
            avdtp_config_atrac_set_sampling_frequency(media_codec_config_data, new_sampling_frequency);
            break;
        default:
            // The console prints no failure for this case.
            printf("Reconfigure not implemented for %s\n", codec_name_for_type(media_codec_type));
            return ERROR_CODE_SUCCESS;
    }
    new_configuration.media_codec.media_type = AVDTP_AUDIO;
    new_configuration.media_codec.media_codec_type = media_codec_type;
    new_configuration.media_codec.media_codec_information_len = media_codec_config_len;
    new_configuration.media_codec.media_codec_information = media_codec_config_data;
    return avdtp_source_reconfigure(media_session_cid(), media_session_local_seid(), media_session_remote_seid(), 1 << AVDTP_MEDIA_CODEC, new_configuration);
}

int a2dp_codec_configuration_maximum_aac_bitrate(void) {
    return max_aac_bit_rate;
}
