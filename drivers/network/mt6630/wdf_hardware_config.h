/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Provisioning gate: keep disabled until board options and the WMT bundle
 * digest have documented provenance. Never infer ownership from this flag.
 */
#ifndef A04_WIFI_WDF_HARDWARE_CONFIG_H
#define A04_WIFI_WDF_HARDWARE_CONFIG_H
#define A04_WIFI_WDF_CONFIG_VERIFIED 0
static const uint8_t a04_wifi_wdf_wmt_sha256[32] = {0};
static const uint32_t a04_wifi_wdf_ocr = 0;
static const struct a04_wmt_startup_options a04_wifi_wdf_options = {0};
#endif
