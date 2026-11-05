/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_FIRMWARE_H
#define A04_WINDOWS_FIRMWARE_H
#include <ntddk.h>
#include "wmt_bridge.h"
/* Reads fixed driver-owned paths at PASSIVE_LEVEL. Caller must supply a
 * provenance-pinned WMT digest; firmware and calibration are never embedded.
 * This API does not establish a lease, regulatory state or NDIS readiness. */
int a04_windows_firmware_boot(struct a04_wmt_bridge *,const uint8_t expected_wmt[32],
 const struct a04_wmt_startup_options *);
#endif
