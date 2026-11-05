/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_M1_11_WINDOWS_COMMAND_H
#define A04_VENDOR_M1_11_WINDOWS_COMMAND_H
#include "windows_start.h"
/* PASSIVE_LEVEL, fixed GP/TA/3D fence only. The caller holds the device's
 * power/lifecycle lock and must prove that the main common buffer is either
 * uncached or hardware coherent for concurrent CPU/FW CCB control fields.
 * AllocateCommonBuffer(..., FALSE) is not that proof on Windows ARM. */
NTSTATUS a04_vendor_m1_11_windows_fence(struct a04_vendor_start *start,
    uint32_t data_master,BOOLEAN coherent_or_uncached);
#endif
