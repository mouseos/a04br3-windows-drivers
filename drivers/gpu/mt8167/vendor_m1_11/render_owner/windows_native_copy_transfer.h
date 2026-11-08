/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_NATIVE_COPY_TRANSFER_H
#define A04_WINDOWS_NATIVE_COPY_TRANSFER_H
#include "windows_native_copy_storage.h"
struct a04_native_tq_transfer;
struct a04_vendor_start;
struct a04_native_irq_core;
struct a04_native_irq_receipt;
struct a04_native_copy_gpu_result {ULONG phase,task_sequence,flush_sequence,client_write;};
/* Internal low-level TQ half. Factory serializes and retains the graph,
 * storage and BOTH real GPU SG leases. This API is NOT wired to WDDM yet.
 * Prepare before FW starts; submit under actual registered Dxgk IRQ lock.
 * Existing single IRQ claims source; PASSIVE finish requires that claim.
 * No second IRQ, CPU pixel read, LockCb, or scheduler notification occurs. */
NTSTATUS a04_native_tq_transfer_prepare(struct a04_native_tq_transfer **,
 struct a04_native_copy_storage *);
NTSTATUS a04_native_tq_transfer_submit(struct a04_native_tq_transfer *,
 struct a04_vendor_start *,struct a04_native_irq_core *,ULONG expected_kernel_write);
/* Capture only an actual ISR receipt under the existing Dxgk IRQ lock.
 * No q->serial/64-bit state is sampled by the PASSIVE worker. */
NTSTATUS a04_native_tq_transfer_begin_finish(struct a04_native_tq_transfer *,
 struct a04_native_irq_core *,const struct a04_native_irq_receipt *);
NTSTATUS a04_native_tq_transfer_finish_gpu(struct a04_native_tq_transfer *,
 struct a04_vendor_start *,struct a04_native_copy_gpu_result *);
/* Only never-submitted cold objects can be freed here. Submitted/ambiguous
 * objects and command_busy remain retained until full CPU-ownership + fresh
 * real IRQ/fence integration (or proven owner DMA-off retirement) is added.
 * GPU_RESULT is NOT CPU_VISIBLE and cannot complete a WDDM fence. */
NTSTATUS a04_native_tq_transfer_release_unsubmitted(struct a04_native_tq_transfer **);
#endif
