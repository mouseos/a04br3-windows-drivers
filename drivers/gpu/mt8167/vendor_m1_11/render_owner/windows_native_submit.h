/* SPDX-License-Identifier: MIT */
#ifndef A04_WINDOWS_NATIVE_SUBMIT_H
#define A04_WINDOWS_NATIVE_SUBMIT_H
#include "windows_native_cold.h"
struct a04_vendor_start;
/* Internal provider primitive, NOT geo_owner_ops.submit yet. Caller owns the
 * real lifetime and command_busy lease, staged pair, backing and queue state.
 * Fixed-size checks/writes only: no pool, mutex, sleep, stall, fence polling,
 * CPU mapping or completion callback. Success means accepted, not complete.
 * Future DIRQL caller must use the real GPU interrupt synchronization owner.
 * A failed/ambiguous publication requires poison and DMA retention by caller. */
NTSTATUS a04_vendor_native_submit_prepared_pair(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,uint32_t expected_kernel_write);
#endif
