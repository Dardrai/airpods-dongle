// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_A2DP_CODEC_IDENTIFIERS_H
#define AIRPODS_A2DP_CODEC_IDENTIFIERS_H
#include <stdint.h>

// Vendor-specific (non-A2DP) codec identity, shared by the codec configuration
// and the per-packet media path. Kept inline so the media path does not gain calls.
#define A2DP_CODEC_VENDOR_ID_APPLE 0x004C
#define A2DP_APPLE_CODEC_AAC_ELD 0x8001

static inline uint32_t get_vendor_id(const uint8_t *codec_info) {
    uint32_t vendor_id = 0;
    vendor_id |= codec_info[0];
    vendor_id |= codec_info[1] << 8;
    vendor_id |= codec_info[2] << 16;
    vendor_id |= (uint32_t)codec_info[3] << 24;
    return vendor_id;
}

static inline uint16_t get_codec_id(const uint8_t *codec_info) {
    uint16_t codec_id = 0;
    codec_id |= codec_info[4];
    codec_id |= codec_info[5] << 8;
    return codec_id;
}

#endif
