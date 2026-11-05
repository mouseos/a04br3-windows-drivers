/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_M1_11_WINDOWS_STOP_H
#define A04_VENDOR_M1_11_WINDOWS_STOP_H
#include "windows_start.h"
/* Caller holds lifecycle lock, has drained FW-reported interrupt counters,
 * owns a powered mapped register range and has stopped all new work.
 * A successful register sequence does NOT prove platform fabric drain and
 * never releases hardware-owned common buffers. */
NTSTATUS a04_vendor_m1_11_windows_stop(struct a04_vendor_start *start,
    BOOLEAN interrupts_drained,ULONG *failed_register,uint64_t *last_value);
#endif
