/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_FIRMWARE_H
#define A04_WINDOWS_FIRMWARE_H
#include <ntddk.h>
#include "wmt_bridge.h"
/* Reads fixed driver-owned paths at PASSIVE_LEVEL. Caller must supply a
 * provenance-pinned WMT digest; firmware and calibration are never embedded.
 * This API does not establish a lease, regulatory state or NDIS readiness. */
struct a04_windows_fw_snapshot { uint8_t *wmt,*wlan; ULONG wmt_length,wlan_length; };
int a04_windows_firmware_prepare(struct a04_windows_fw_snapshot *,const uint8_t [32]);
int a04_windows_firmware_prepare_wmt(struct a04_windows_fw_snapshot *,const uint8_t [32]);
void a04_windows_firmware_release(struct a04_windows_fw_snapshot *);
int a04_windows_firmware_boot_prepared(struct a04_wmt_bridge *,struct a04_windows_fw_snapshot *,const uint8_t [32],const struct a04_wmt_startup_options *);
int a04_windows_firmware_boot(struct a04_wmt_bridge *,const uint8_t expected_wmt[32],
 const struct a04_wmt_startup_options *);
#endif
