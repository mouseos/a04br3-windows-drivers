/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Provisioning gate: keep disabled until board options and the WMT bundle
 * digest have documented provenance. Never infer ownership from this flag.
 */
#ifndef A04_WIFI_WDF_HARDWARE_CONFIG_H
#define A04_WIFI_WDF_HARDWARE_CONFIG_H
#define A04_WIFI_WDF_CONFIG_VERIFIED 0
/* A04WMT1 bundle assembled from pinned vendor revision 0c756f413e7011231c8fb793af0930311023f68d.
 * Digest identifies inputs only; it does not prove board settings or ownership. */
static const uint8_t a04_wifi_wdf_wmt_sha256[32] = {
 0x4a,0xf7,0x99,0x6b,0x89,0xef,0x11,0x0d,0x8f,0x27,0x08,0x77,0xf6,0x1a,0xaf,0x0b,
 0x90,0xfd,0xe2,0x1b,0x6c,0x5b,0x0e,0xd2,0x3c,0xcb,0xd3,0x2c,0x29,0x89,0xdc,0xee
};
static const uint32_t a04_wifi_wdf_ocr = 0;
static const struct a04_wmt_startup_options a04_wifi_wdf_options = {0};
#endif
