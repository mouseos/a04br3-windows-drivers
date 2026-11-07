/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_M111_WINDOWS_NATIVE_COLD_H
#define MT8167_M111_WINDOWS_NATIVE_COLD_H
#include <ntddk.h>
#include <stdint.h>
#define A04_NATIVE_DATA_VA UINT64_C(0x65c0200000)
#define A04_NATIVE_DATA_LIMIT 0x200000u
struct a04_vendor_native_cold;
struct a04_vendor_render_cold;
struct a04_m111_hwrt_input;
/* Returned even when the child was fully unwound; no raw CPU pointer. */
struct a04_vendor_native_diagnostic {
    ULONG stage,index,request_bytes,map_registers,data_bytes,data_blocks,pixel_blocks;
    NTSTATUS status;
};
#define A04_NATIVE_PREP_OWNER 1u
#define A04_NATIVE_PREP_CAPACITY 2u
#define A04_NATIVE_PREP_DATA 3u
#define A04_NATIVE_PREP_PT 4u
#define A04_NATIVE_PREP_SHADOW 5u
#define A04_NATIVE_PREP_MAP 6u
#define A04_NATIVE_PREP_BANK 7u
#define A04_NATIVE_PREP_CODE 8u
#define A04_NATIVE_PREP_OUTPUT 9u
#define A04_NATIVE_PREP_JOB 10u
#define A04_NATIVE_PREP_DONE 11u
NTSTATUS a04_vendor_native_allocate_cold_ex(struct a04_vendor_native_cold **,
    struct a04_vendor_render_cold *,PDMA_ADAPTER,ULONG,ULONG,ULONG,
    struct a04_vendor_native_diagnostic *);
/* Opaque boundary: m1.11 DDK types must not enter the modern FWIF provider TU.
 * The allocation is retained once the parent is hardware-owned. */
NTSTATUS a04_vendor_native_allocate_cold(struct a04_vendor_native_cold **,
    struct a04_vendor_render_cold *,PDMA_ADAPTER,ULONG map_registers,
    ULONG width,ULONG height);
NTSTATUS a04_vendor_native_release_cold(struct a04_vendor_native_cold **);
ULONG a04_vendor_native_bytes(const struct a04_vendor_native_cold *);
/* Complete CPU candidate and cold ring image staged; does not open publish gate. */
BOOLEAN a04_vendor_native_job_staged(const struct a04_vendor_native_cold *);
struct a04_vendor_start;
struct a04_vendor_native_result {uint32_t phase,matching_pixels,mismatched_pixels,guard_errors,completed_frames;};
/* Source-linked private activation diagnostic only. Existing result, DOD/copy
 * interface and WDDM owner ABI are unchanged. Values come from the existing
 * completed-DMA scan; valid bits distinguish unread/partial data. */
struct a04_vendor_native_scan_sample {uint32_t index,expected,observed;};
struct a04_vendor_native_scan_diagnostic {
    uint32_t started,failed,frame_index,scanned_pixels,scanned_words;
    uint32_t colored_pixels,black_pixels,other_pixels;
    uint32_t first_valid,first_index,first_expected,first_observed,first_is_guard;
    uint32_t sample_mask;
    struct a04_vendor_native_scan_sample sample[5]; /* TL/TR/BL/BR/centre */
};
BOOLEAN a04_vendor_native_failed_scan(const struct a04_vendor_native_cold *,
    struct a04_vendor_native_scan_diagnostic *);

/* Activation-only first private clear. Caller holds the provider lifetime
 * and pinned copy owner, with no display lease and GP fence already passed. */
NTSTATUS a04_vendor_native_run_once(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,struct a04_vendor_native_result *);
/* One bounded private run, 1..16 frames. Same clear/code/owner; no arbitrary
 * packet entry. Requires the same lifetime/admission as run_once. */
NTSTATUS a04_vendor_native_run_clear_frames(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,ULONG,struct a04_vendor_native_result *);
struct a04_native_mdl_binding;
/* Concrete reservation from this native owner's existing PD/PTs. This is a
 * GPU aperture, never a guessed CPU BAR or physical identity mapping. Current
 * owner admits one800x600 full-MDL cold binding; no live mapping capability. */
struct a04_native_target_aperture {uint64_t gpu_base,bytes,commit_limit,highest_dma_address;};
NTSTATUS a04_vendor_native_target_aperture(const struct a04_vendor_native_cold *,
    struct a04_native_target_aperture *);
NTSTATUS a04_vendor_native_attach_mdl_target_at_cold(struct a04_vendor_native_cold *,
    struct a04_native_mdl_binding *,uint64_t aperture_byte_offset,uint32_t packed_argb);
/* Standard owner preparation preserves active pixels until actual submit;
 * only the rounded-MDL padding guard is initialized. Legacy diagnostics keep
 * their full-image sentinel and are unchanged. Colour rebuild is cold-only. */
NTSTATUS a04_vendor_native_attach_mdl_target_preserve_cold(struct a04_vendor_native_cold *,
    struct a04_native_mdl_binding *,uint64_t,uint32_t);
NTSTATUS a04_vendor_native_recolour_bound_cold(struct a04_vendor_native_cold *,uint32_t);
/* Native owner split path: submit only from the actual registered Dxgk
 * interrupt synchronization lock, retained D0/MDL lease and cold-prepared
 * mapped target. Accepted != complete. command_busy stays held until finish.
 * finish is PASSIVE SLC/GP/MDL cache + readback, NOT a DIRQL notification or
 * quiescence receipt. GEO visibility still needs a subsequent real HW IRQ. */
NTSTATUS a04_vendor_native_owner_submit(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,uint32_t expected_kernel_write);
NTSTATUS a04_vendor_native_owner_finish_passive(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,struct a04_vendor_native_result *);
struct a04_native_irq_core;
struct a04_native_owner_event {
    uint64_t epoch,job,irq_serial;
    uint32_t generation;NTSTATUS status;BOOLEAN claimed,completed;
};
/* Both calls only under the REAL Dxgk IRQ synchronization/ISR lock.
 * arm runs AFTER successful PASSIVE finish/cache and queues a fresh GP SYNC.
 * observer reads and claims a real MIPS IRQ source and requires that fresh
 * fence, queue drain and matching epoch before setting completed. A passive
 * poll/raised IRQL is never a completion notification. No callback here. */
NTSTATUS a04_vendor_native_owner_arm_visibility(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,struct a04_native_irq_core *,uint64_t epoch,
    uint64_t job,uint32_t generation);
BOOLEAN a04_vendor_native_owner_interrupt_event(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,struct a04_native_irq_core *,struct a04_native_owner_event *);
/* Optional source-linked miniport profile: never started firmware, same
 * validated800x600 job, full rounded locked-MDL extent. No IRQ/cap publish. */
NTSTATUS a04_vendor_native_attach_mdl_target_cold(struct a04_vendor_native_cold *,struct a04_native_mdl_binding *);
NTSTATUS a04_vendor_native_attach_vertex_cold(struct a04_vendor_native_cold *,
    struct a04_native_mdl_binding *,uint64_t);
NTSTATUS a04_vendor_native_target_before_fw_start(struct a04_vendor_native_cold *);
/* Independently derived HWRT register input; bank bind is not drawing. */
NTSTATUS a04_vendor_native_bind_bank_cold(struct a04_vendor_native_cold *,
    const struct a04_m111_hwrt_input *);
#endif
