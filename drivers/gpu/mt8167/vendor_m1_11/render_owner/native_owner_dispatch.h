/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OWNER_DISPATCH_H
#define A04_NATIVE_OWNER_DISPATCH_H
#include "windows_native_cold.h"
#include "native_owner_irq.h"
/* Source-linked control-v2 execution bridge, NOT a factory/lease or caps.
 * Storage and all pointed-to objects are resident and retained by miniport.
 * synchronize MUST invoke routine under the actual Dxgk interrupt lock.
 * No KMDF IRQ, raised-IRQL substitute, or completion callback is created. */
typedef BOOLEAN (*a04_native_locked_routine)(void *);
typedef BOOLEAN (*a04_native_synchronize)(void *,a04_native_locked_routine,void *);
#define A04_NATIVE_DISPATCH_READY 1u
#define A04_NATIVE_DISPATCH_SUBMITTED 2u
#define A04_NATIVE_DISPATCH_FINISHING 3u
#define A04_NATIVE_DISPATCH_ARMED 4u
#define A04_NATIVE_DISPATCH_COMPLETE 5u
#define A04_NATIVE_DISPATCH_FAULT 6u
struct a04_native_owner_dispatch {
    struct a04_vendor_native_cold *native;
    struct a04_vendor_start *start;
    struct a04_native_irq_core *irq;
    void *sync_context;a04_native_synchronize synchronize;
    uint64_t epoch,job;uint32_t generation,state;
    NTSTATUS failure;
    struct a04_vendor_native_result result;
};
/* PASSIVE with exclusive lifecycle ownership, only on zeroed fresh storage.
 * Caller already owns real D0, exact MDL lease, mappings and sole Dxgk IRQ.
 * This retains nothing itself; it must not outlive those references. */
NTSTATUS a04_native_dispatch_rearm_completed(struct a04_native_owner_dispatch *);
NTSTATUS a04_native_dispatch_initialize(struct a04_native_owner_dispatch *,
    struct a04_vendor_native_cold *,struct a04_vendor_start *,
    struct a04_native_irq_core *,uint64_t,void *,a04_native_synchronize);
/* Actual Dxgk IRQ lock. One already cold-prepared job. Does not prepare live
 * packets, release bindings, reset, certify quiescence or enable caps. */
NTSTATUS a04_native_dispatch_submit(struct a04_native_owner_dispatch *,uint64_t,
    uint32_t,uint32_t expected_kernel_write);
/* Actual registered ISR only. TRUE = source claimed, NOT necessarily done.
 * Only completed=TRUE can be translated to GEO GPU_DONE|CPU_VISIBLE. */
BOOLEAN a04_native_dispatch_interrupt(struct a04_native_owner_dispatch *,
    struct a04_native_owner_event *);
/* Serialized PASSIVE worker after claimed/masked source. Completes native
 * fence/cache then arms fresh GP through actual interrupt synchronization.
 * A pre-fence IRQ in ARMED state only rearms the source. No notification.
 * FWCCB requests need real outer service before retry if rearm returns busy. */
NTSTATUS a04_native_dispatch_service(struct a04_native_owner_dispatch *);
#endif
