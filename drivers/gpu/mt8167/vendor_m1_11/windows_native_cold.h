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
/* Activation-only first private clear. Caller holds the provider lifetime
 * and pinned copy owner, with no display lease and GP fence already passed. */
NTSTATUS a04_vendor_native_run_once(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,struct a04_vendor_native_result *);
/* One bounded private run, 1..16 frames. Same clear/code/owner; no arbitrary
 * packet entry. Requires the same lifetime/admission as run_once. */
NTSTATUS a04_vendor_native_run_clear_frames(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,ULONG,struct a04_vendor_native_result *);
/* Independently derived HWRT register input; bank bind is not drawing. */
NTSTATUS a04_vendor_native_bind_bank_cold(struct a04_vendor_native_cold *,
    const struct a04_m111_hwrt_input *);
#endif
