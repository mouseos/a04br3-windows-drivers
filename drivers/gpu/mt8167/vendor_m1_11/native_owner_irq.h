/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OWNER_IRQ_H
#define A04_NATIVE_OWNER_IRQ_H
#include <ntddk.h>
#include <stdint.h>
struct a04_rgx_fw_runtime;
/* Embedded in the exclusive Dxgk miniport adapter, resident/nonpageable.
 * No WDF interrupt or callback is created. All MMIO functions below run under
 * the miniport's actual interrupt synchronization lock. bind/unbind also need
 * adapter lifecycle serialization and real OS callback disconnect/rundown. */
struct a04_native_irq_core {
    volatile UCHAR *core;ULONG bytes;struct a04_rgx_fw_runtime *runtime;
    uint64_t epoch,serial;BOOLEAN powered,masked,closed,faulted;
};
struct a04_native_irq_receipt {uint64_t epoch,serial;ULONG status,ack_status;NTSTATUS fault;};
NTSTATUS a04_native_irq_bind(struct a04_native_irq_core *,volatile UCHAR *,ULONG,
    struct a04_rgx_fw_runtime *,uint64_t epoch);
/* Only from the registered DxgkDdiInterruptRoutine. A receipt proves a real
 * pending device source was sampled/masked/acknowledged, NOT GPU completion
 * or CPU visibility. fault!=0/serial==0 is a source failure, never a completion. Caller routes deferred service through its own DDI. */
BOOLEAN a04_native_irq_claim(struct a04_native_irq_core *,struct a04_native_irq_receipt *);
/* After actual FWCCB service, recheck its empty ring and source deassertion.
 * Old epoch/receipt cannot rearm a newer interrupt or a reset adapter. */
NTSTATUS a04_native_irq_rearm(struct a04_native_irq_core *,uint64_t epoch,uint64_t serial);
NTSTATUS a04_native_irq_mask_close(struct a04_native_irq_core *,uint64_t epoch);
/* After real OS disconnect+rundown, with the source closed. Does not free
 * DMA, certify GPU drain, reset hardware or produce GEO_PROOF_REQUIRED. */
NTSTATUS a04_native_irq_unbind_disconnected(struct a04_native_irq_core *);
#endif
