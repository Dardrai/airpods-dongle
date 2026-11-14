/*
 * AVDTP source test console on the BTstack stdin (HAVE_BTSTACK_STDIN in
 * btstack_config.h). Split out of btstack_avdtp_source.c; main/Bluetooth
 * context only.
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
#include <stdio.h>
#include "btstack.h"
#include "btstack_avdtp_source.h"
#include "btstack_hci.h"
#include "a2dp_codec_configuration.h"
#include "media_session.h"
#include "bluetooth_debug_console.h"

#ifdef HAVE_BTSTACK_STDIN

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP SOURCE Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - create connection to addr %s\n", get_device_addr_string());
    printf("C      - disconnect\n");
    printf("D      - Delete linked list\n");
    printf("z      - select remote endpoint\n");
    printf("g      - set_next_capablity_and_start_stream  %u\n", media_session_remote_seid());
    printf("a      - get all capabilities for remote seid %u\n", media_session_remote_seid());
    printf("s      - set configuration    for remote seid %u\n", media_session_remote_seid());
    printf("f      - get configuration    for remote seid %u\n", media_session_remote_seid());
    printf("R      - reconfigure stream   for remote seid %u\n", media_session_remote_seid());
    printf("o      - open stream          for remote seid %u\n", media_session_remote_seid());
    printf("m      - start stream         for remote seid %u\n", media_session_remote_seid());
    printf("A      - abort stream         for remote seid %u\n", media_session_remote_seid());
    printf("S      - stop stream          for remote seid %u\n", media_session_remote_seid());
    printf("P      - suspend stream       for remote seid %u\n", media_session_remote_seid());
    printf("l      - set up ladc          for remote seid %u\n", media_session_remote_seid());
    printf("u      - set up sbc           for remote seid %u\n", media_session_remote_seid());
    printf("Ctrl-c - exit\n");
    printf("---\n");
}


static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    media_session_mark_local_command();
    static bool enter_remote_seid_index = false;

    if (enter_remote_seid_index){
        if ((cmd < '0') || (cmd > '9')) return;
        uint8_t index = cmd - '0';
        if (index >= a2dp_codec_configuration_remote_endpoint_count()){
            printf("Index too high, try again\n");
            return;
        }
        enter_remote_seid_index = false;
        a2dp_codec_configuration_select_remote_endpoint(index);
        printf("Selected Remote Stream Endpoint with SEID %u\n",  media_session_remote_seid());
        return;
    }

    switch (cmd){
        case 'c':
            printf("Establish AVDTP Source connection to %s\n", get_device_addr_string());
            status = media_session_connect_signaling();
            break;
        case 'C':
            printf("Disconnect AVDTP Source\n");
            status = avdtp_source_disconnect(media_session_cid());
            break;
        case 'D':
            printf("Deleting all link keys\n");
            gap_delete_all_link_keys();
            printf("Finished\n");
            break;
        case 'g':
            printf("set_next_capablity_and_start_stream with seid %d\n", media_session_remote_seid());
            set_next_capablity_and_start_stream();
            break;
        case 'a':
            printf("Get all capabilities of stream endpoint with seid %d\n", media_session_remote_seid());
            status = avdtp_source_get_all_capabilities(media_session_cid(), media_session_remote_seid());
            break;
        case 'f':
            printf("Get configuration of stream endpoint with seid %d\n", media_session_remote_seid());
            status = avdtp_source_get_configuration(media_session_cid(), media_session_remote_seid());
            break;
        case 'z':
            if (a2dp_codec_configuration_remote_endpoint_count() == 0){
                printf("Remote Stream Endpoints not discovered yet, please discover stream endpoints first\n");
                break;
            }
            a2dp_codec_configuration_print_remote_endpoints();
            printf("Please enter index of remote stream endpoint:\n");
            enter_remote_seid_index = true;
            break;

        case 's':{
            //status = set_configuration();
            break;
        }
        case 'R':
            status = a2dp_codec_configuration_reconfigure_selected_endpoint();
            break;
        case 'o':
            printf("Establish stream between local %d and remote %d seid\n", media_session_local_seid(), media_session_remote_seid());
            status = avdtp_source_open_stream(media_session_cid(), media_session_local_seid(), media_session_remote_seid());
            break;
        case 'm':
            printf("Start stream between local %d and remote %d seid, \n", media_session_local_seid(), media_session_remote_seid());
            status = avdtp_source_start_stream(media_session_cid(), media_session_local_seid());
            break;
        case 'A':
            printf("Abort stream between local %d and remote %d seid\n", media_session_local_seid(), media_session_remote_seid());
            status = avdtp_source_abort_stream(media_session_cid(), media_session_local_seid());
            break;
        case 'S':
            printf("Release stream between local %d and remote %d seid\n", media_session_local_seid(), media_session_remote_seid());
            status = avdtp_source_stop_stream(media_session_cid(), media_session_local_seid());
            break;
        case 'P':
            printf("Susspend stream between local %d and remote %d seid\n", media_session_local_seid(), media_session_remote_seid());
            status = avdtp_source_suspend(media_session_cid(), media_session_local_seid());
            break;
        case 'X':
            printf("Stop streaming\n");
            status = (media_session_cid(), media_session_local_seid());
            break;


        case 'v':
            increase_vol_by_key();
            break;
        case 'V':
            decrease_vol_by_key();
            break;

        case 'p':
            media_session_send_media_packet();
            break;

        case 'u':
            printf("Setup SBC codec\n");
            status = setup_sbc_configuration();
            break;

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
    if (status != ERROR_CODE_SUCCESS){
        printf("AVDTP Sink cmd \'%c\' failed, status 0x%02x\n", cmd, status);
    }
}

void bluetooth_debug_console_init(void) {
    btstack_stdin_setup(stdin_process);
}
#endif
