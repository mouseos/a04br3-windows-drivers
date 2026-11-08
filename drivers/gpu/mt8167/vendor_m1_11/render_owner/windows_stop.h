/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_M1_11_WINDOWS_STOP_H
#define A04_VENDOR_M1_11_WINDOWS_STOP_H
#include "windows_start.h"
#include "A04BR3RogueCoreResetHold.h"
/* Caller holds lifecycle lock, has drained FW-reported interrupt counters,
 * owns a powered mapped register range and has stopped all new work.
 * A successful register sequence does NOT prove platform fabric drain and
 * never releases hardware-owned common buffers. */
NTSTATUS a04_vendor_m1_11_windows_stop(struct a04_vendor_start *start,
    BOOLEAN interrupts_drained,ULONG *failed_register,uint64_t *last_value);
/* GPU0 RW resource only, after native admission closure, actual FW off ack
 * and host/FW IRQ count drain. Caller already masked source under real Dxgk
 * lock. Records actual reset hold; does not authorize DMA release or GEO caps. */
NTSTATUS a04_vendor_m1_11_windows_core_hold_reset(struct a04_vendor_start *,
    BOOLEAN interrupts_drained,struct a04_rgx_core_reset_observation *);
#endif
