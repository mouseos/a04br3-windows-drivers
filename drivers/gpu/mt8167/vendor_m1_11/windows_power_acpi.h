/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_M1_11_WINDOWS_POWER_ACPI_H
#define A04_VENDOR_M1_11_WINDOWS_POWER_ACPI_H
#include <wdf.h>
#include "windows_start.h"
/* Read-only GPST method on this device's PDO. mfg must be the driver's mapped
 * ACPI-assigned MFG resource. The result is a snapshot, not a power lease. */
NTSTATUS a04_vendor_m1_11_power_snapshot(WDFDEVICE device,volatile UCHAR *mfg,
    ULONG mfg_bytes,struct a04_vendor_power_snapshot *out);
#endif
