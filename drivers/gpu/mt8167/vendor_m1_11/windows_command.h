/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_M1_11_WINDOWS_COMMAND_H
#define A04_VENDOR_M1_11_WINDOWS_COMMAND_H
#include "windows_start.h"
#include "windows_irq_snapshot.h"
#include "A04BR3RogueDDKABI.h"
typedef int (*a04_vendor_request_handler)(void *,const RGXFWIF_FWCCB_CMD *);
/* PASSIVE_LEVEL, caller holds lifecycle lock. A callback returning nonzero
 * leaves that request pending. more is TRUE when the bounded budget expires.
 * The callback must not re-enter this transport or retain the command copy. */
NTSTATUS a04_vendor_m1_11_windows_requests(struct a04_vendor_start *start,
    BOOLEAN coherent_or_uncached,a04_vendor_request_handler handler,void *context,
    uint32_t budget,uint32_t *handled,BOOLEAN *more);
/* PASSIVE_LEVEL, fixed GP/TA/3D fence only. The caller holds the device's
 * power/lifecycle lock and must prove that the main common buffer is either
 * uncached or hardware coherent for concurrent CPU/FW CCB control fields.
 * AllocateCommonBuffer(..., FALSE) is not that proof on Windows ARM. */
NTSTATUS a04_vendor_m1_11_windows_fence(struct a04_vendor_start *start,
    uint32_t data_master,BOOLEAN coherent_or_uncached);
/* Stop accepting work before calling. Firmware acknowledgement is only one
 * step of shutdown: IRQ/fabric drain is not yet implemented, so all DMA
 * buffers stay hardware-owned even after STATUS_SUCCESS. */
NTSTATUS a04_vendor_m1_11_windows_power_off(struct a04_vendor_start *start,
    BOOLEAN coherent_or_uncached);
/* Diagnostic-only, serialized forced-idle then forced OFF. All DMA stays owned. */
NTSTATUS a04_vendor_m1_11_windows_forced_power_off(struct a04_vendor_start *start,
    BOOLEAN coherent_or_uncached,NTSTATUS *idle_status);
#endif
