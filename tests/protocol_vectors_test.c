// SPDX-License-Identifier: GPL-3.0-only
// Checks the production protocol code against protocol/vectors.json, which the
// Go and Kotlin clients are meant to share. tools/test.py generates the include.
#include <assert.h>
#include <stdio.h>
#include "../src/btstack/dongle_settings.h"
#include "../src/btstack/phone_control_protocol.h"
#include "../src/btstack/playback_options.h"
#include "../src/btstack/volume_authority.h"
#include "protocol_vectors.inc"

static uint16_t read16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static void check(bool condition, const char *section, const char *name) {
    if (!condition) {
        fprintf(stderr, "protocol vector failed: %s: %s\n", section, name);
        assert(false);
    }
}

static void control_commands(void) {
    for (unsigned i = 0; i < VECTOR_COUNT(command_vectors); ++i) {
        const command_vector_t *v = &command_vectors[i];
        check(dongle_command_valid(v->data, v->size) == v->valid, "control command validity", v->name);
        if (!v->valid) continue;
        const uint8_t *p = v->data;
        check(p[DONGLE_COMMAND_OFFSET_OPCODE] == v->opcode &&
              dongle_u32(p + DONGLE_COMMAND_OFFSET_CONNECTION_GENERATION) == v->connection_generation &&
              !memcmp(p + DONGLE_COMMAND_OFFSET_SELECTED_ADDRESS, v->selected_address,
                      DONGLE_COMMAND_SIZE_SELECTED_ADDRESS) &&
              dongle_u32(p + DONGLE_COMMAND_OFFSET_COMMAND_SEQUENCE) == v->command_sequence &&
              p[DONGLE_COMMAND_OFFSET_ARGUMENT] == v->argument, "control command fields", v->name);
    }
}

// Encode each status the way dongle_control_poll() does and compare the bytes.
static void control_status(void) {
    for (unsigned i = 0; i < VECTOR_COUNT(status_vectors); ++i) {
        const status_vector_t *v = &status_vectors[i];
        uint8_t report[DONGLE_REPORT_SIZE];
        dongle_header(report);
        report[DONGLE_STATUS_OFFSET_FLAGS] = v->flags;
        dongle_put32(report + DONGLE_STATUS_OFFSET_CONNECTION_GENERATION, v->connection_generation);
        memcpy(report + DONGLE_STATUS_OFFSET_SELECTED_ADDRESS, v->selected_address, DONGLE_STATUS_SIZE_SELECTED_ADDRESS);
        report[DONGLE_STATUS_OFFSET_LISTENING_MODE] = v->listening_mode;
        memcpy(report + DONGLE_STATUS_OFFSET_BATTERY_PERCENT, v->battery_percent, DONGLE_STATUS_SIZE_BATTERY_PERCENT);
        report[DONGLE_STATUS_OFFSET_CHARGING_COMPONENTS] = v->charging_components;
        report[DONGLE_STATUS_OFFSET_OUTPUT_CODEC] = v->output_codec;
        for (unsigned slot = 0; slot < 3; ++slot)
            dongle_put32(report + DONGLE_STATUS_OFFSET_BATTERY_AGES_MILLISECONDS + slot * 4,
                         v->battery_ages_milliseconds[slot]);
        dongle_put32(report + DONGLE_STATUS_OFFSET_LAST_COMMAND_SEQUENCE, v->last_command_sequence);
        report[DONGLE_STATUS_OFFSET_LAST_COMMAND_RESULT] = v->last_command_result;
        report[DONGLE_STATUS_OFFSET_LAST_COMMAND_OPCODE] = v->last_command_opcode;
        report[DONGLE_STATUS_OFFSET_CAPABILITIES] = (uint8_t)v->capabilities;
        report[DONGLE_STATUS_OFFSET_CAPABILITIES + 1] = (uint8_t)(v->capabilities >> 8);
        report[DONGLE_STATUS_OFFSET_PRESENT_COMPONENTS] = v->present_components;
        report[DONGLE_STATUS_OFFSET_CASE_OBSERVED_COMPONENTS] = v->case_observed_components;
        check(v->size == DONGLE_REPORT_SIZE && !memcmp(report, v->data, DONGLE_REPORT_SIZE), "control status", v->name);
    }
    // Every published result code appears in at least one status vector.
    for (unsigned code = DONGLE_RESULT_NONE; code <= DONGLE_RESULT_MALFORMED; ++code) {
        bool seen = false;
        for (unsigned i = 0; i < VECTOR_COUNT(status_vectors); ++i)
            seen |= status_vectors[i].last_command_result == code;
        check(seen, "control status coverage", "result code missing");
    }
}

static void rpc_frames(void) {
    unsigned operations = 0, errors = 0;
    for (unsigned i = 0; i < VECTOR_COUNT(rpc_vectors); ++i) {
        const rpc_vector_t *v = &rpc_vectors[i];
        const uint8_t *p = v->data;
        check(v->size == DONGLE_REPORT_SIZE && dongle_rpc_header_valid(p) == v->request_valid, "rpc header", v->name);
        check(p[DONGLE_RPC_OFFSET_OPERATION] == v->operation &&
              dongle_u32(p + DONGLE_RPC_OFFSET_CONNECTION_GENERATION) == v->connection_generation &&
              dongle_u32(p + DONGLE_RPC_OFFSET_TRANSACTION_SEQUENCE) == v->transaction_sequence &&
              read16(p + DONGLE_RPC_OFFSET_TOTAL_LENGTH) == v->total_length &&
              read16(p + DONGLE_RPC_OFFSET_FRAGMENT_OFFSET) == v->fragment_offset &&
              p[DONGLE_RPC_OFFSET_FRAGMENT_LENGTH] == v->fragment_length &&
              p[DONGLE_RPC_OFFSET_RESPONSE_PHASE] == v->response_phase &&
              !memcmp(p + DONGLE_RPC_OFFSET_PAYLOAD, v->payload, v->payload_size), "rpc fields", v->name);
        if (v->request_valid) operations |= 1u << v->operation;
        if (v->response_phase == DONGLE_RPC_PHASE_FAILED) errors |= 1u << v->payload[0];
    }
    for (unsigned op = DONGLE_RPC_OPERATION_SEND_ATT_REQUEST; op <= DONGLE_RPC_OPERATION_TYPED_FEATURE_REQUEST; ++op)
        check(operations & (1u << op), "rpc coverage", "operation missing");
    for (unsigned code = DONGLE_RPC_ERROR_MALFORMED; code <= DONGLE_RPC_ERROR_CANCELLED; ++code)
        check(errors & (1u << code), "rpc coverage", "error code missing");
}

static void features(void) {
    for (unsigned i = 0; i < VECTOR_COUNT(feature_vectors); ++i) {
        const feature_vector_t *v = &feature_vectors[i];
        check(v->size && v->data[0] == v->feature, "feature id", v->name);
        if (!v->writes_setting) continue;
        check(v->feature == DONGLE_FEATURE_WRITE_SETTING && v->size == 6 && v->data[1] == v->setting &&
              dongle_u32(v->data + 2) == v->value && setting_value_valid(v->setting, v->value),
              "feature setting write", v->name);
    }
}

static void readback(void) {
    for (unsigned i = 0; i < VECTOR_COUNT(readback_vectors); ++i) {
        const readback_vector_t *v = &readback_vectors[i];
        dongle_readback_t state;
        dongle_readback_clear(&state);
        dongle_readback_packet(&state, v->data, v->size, 50);
        bool known = true;
        for (unsigned slot = 0; slot < 3; ++slot) known &= state.known[slot] == v->known[slot];
        check(state.anc == v->listening_mode && !memcmp(state.battery, v->battery_percent, 3) &&
              state.charging == v->charging_components && known, "aacp readback", v->name);
    }
}

static void settings_sessions(void) {
    for (unsigned i = 0; i < VECTOR_COUNT(session_vectors); ++i) {
        const session_vector_t *v = &session_vectors[i];
        dongle_settings_t state;
        settings_reset(&state);
        for (unsigned packet = 0; packet < v->count; ++packet)
            settings_packet(&state, v->packets[packet].data, v->packets[packet].size, v->packets[packet].at);
        uint8_t snapshot[DONGLE_SETTINGS_SNAPSHOT_HEADER_SIZE + DONGLE_SETTING_COUNT * DONGLE_SETTINGS_SNAPSHOT_RECORD_SIZE];
        uint16_t length = settings_snapshot(&state, v->now, snapshot, sizeof(snapshot), v->extended);
        check(length == v->snapshot_size && !memcmp(snapshot, v->snapshot, length), "settings session", v->name);
    }
}

static void setting_values(void) {
    for (unsigned i = 0; i < VECTOR_COUNT(value_vectors); ++i) {
        const value_vector_t *v = &value_vectors[i];
        char name[48];
        snprintf(name, sizeof(name), "setting 0x%02x value 0x%x", v->setting, (unsigned)v->value);
        check(setting_value_valid(v->setting, v->value) == v->valid, "setting value", name);
    }
}

static unsigned phone_requests;
static bool count_request(void *context, const uint8_t *frame) {
    (void)context; (void)frame;
    ++phone_requests;
    return true;
}
static void phone_frames(void) {
    for (unsigned i = 0; i < VECTOR_COUNT(phone_vectors); ++i) {
        const phone_vector_t *v = &phone_vectors[i];
        phone_control_parser_t parser = {0};
        phone_requests = 0;
        bool accepted = phone_control_feed(&parser, v->data, (uint16_t)v->size, count_request, NULL);
        check(accepted == v->valid && phone_requests == v->requests, "phone frame", v->name);
    }
}

// Encode each typed feature layout from its generated field offsets and compare the bytes; requests
// that state their validity also go through the production validators.
static void layouts(void) {
    unsigned validity_checks = 0;
    for (unsigned i = 0; i < VECTOR_COUNT(layout_vectors); ++i) {
        const layout_vector_t *v = &layout_vectors[i];
        uint8_t frame[DONGLE_PERFORMANCE_SIZE] = {0};
        check(v->size == v->expected_size && v->size <= sizeof(frame), "layout size", v->name);
        for (unsigned f = 0; f < v->count; ++f)
            for (unsigned byte = 0; byte < v->fields[f].size; ++byte)
                frame[v->fields[f].offset + byte] = (uint8_t)(v->fields[f].value >> (8 * byte));
        check(!memcmp(frame, v->data, v->size), "layout bytes", v->name);
        if (!v->checks_validity) continue;
        bool valid = false;
        if (!strcmp(v->layout, "volume_authority")) {
            valid = volume_authority_request_valid(v->data, (uint16_t)v->size);
        } else if (!strcmp(v->layout, "configure_audio_profile")) {
            playback_options_t options = {v->data[DONGLE_CONFIGURE_AUDIO_PROFILE_OFFSET_REQUESTED_CODEC],
                                          v->data[DONGLE_CONFIGURE_AUDIO_PROFILE_OFFSET_RATE_MODE],
                                          dongle_u32(v->data + DONGLE_CONFIGURE_AUDIO_PROFILE_OFFSET_REQUESTED_BITRATE)};
            valid = v->data[DONGLE_CONFIGURE_AUDIO_PROFILE_OFFSET_FEATURE] == DONGLE_FEATURE_CONFIGURE_AUDIO_PROFILE &&
                    playback_options_valid(options);
        } else {
            check(false, "layout validity", v->name);
        }
        check(valid == v->valid, "layout validity", v->name);
        ++validity_checks;
    }
    check(validity_checks > 0, "layout coverage", "no request validity vectors");
}

int main(void) {
    control_commands();
    control_status();
    rpc_frames();
    features();
    readback();
    settings_sessions();
    setting_values();
    phone_frames();
    layouts();
    puts("protocol vectors: commands, status, RPC, features, readback, settings, phone frames and layouts passed");
}
