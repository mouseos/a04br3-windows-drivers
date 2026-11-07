/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_WLAN_FIRMWARE_H
#define A04_MT6630_WLAN_FIRMWARE_H
#include <stddef.h>
#include <stdint.h>
/* Immutable caller-owned snapshot and SHA256 callback. This parses only;
 * the callback must compute SHA256 over the same retained immutable bytes.
 * No firmware bytes, selection fallback or transfer are provided here. */
struct a04_wlan_section { const uint8_t *data; uint32_t address, length; };
struct a04_wlan_firmware { unsigned count,candidate; struct a04_wlan_section section[16]; };
int a04_wlan_firmware_stage(uint32_t chip,uint32_t hardware,uint32_t rom,
 const uint8_t *data,size_t length,int (*sha256)(const uint8_t *,size_t,uint8_t[32]),
 struct a04_wlan_firmware *out);
#endif
