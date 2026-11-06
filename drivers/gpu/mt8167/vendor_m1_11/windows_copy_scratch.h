/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_WINDOWS_COPY_SCRATCH_H
#define A04_VENDOR_WINDOWS_COPY_SCRATCH_H
#include "windows_render_cold.h"
#include "native_copy_state.h"
#include "native_copy_usc.h"
/* Caller serializes all lifecycle operations and retains the adapter, render,
 * firmware and this owner. map_registers is IoGetDmaAdapter output.
 * A prepared child blocks render/firmware release; exposure is sticky.
 * Nothing here authorizes a GPU command or proves PnP removal safety. */
#define A04_VENDOR_COPY_SOURCE_VA (A04_VENDOR_RENDER_SCRATCH_VA+UINT64_C(0x10000))
#define A04_VENDOR_COPY_DEST_VA   (A04_VENDOR_RENDER_SCRATCH_VA+UINT64_C(0x11000))
#define A04_VENDOR_COPY_SHARED_VA (A04_VENDOR_RENDER_SCRATCH_VA+UINT64_C(0x12000))
struct a04_vendor_copy_scratch {
    struct a04_vendor_uncached source,destination,shared;
    struct a04_vendor_render_cold *render;
    struct a04_vendor_copy_state state;
    _Alignas(8) struct a04_vendor_copy_kick_image kick;
    uint32_t eot_pds[16], eot_usc[3], destination_csr[4];
    uint32_t tile_raw04, tile_raw18, tile_raw20;
    uint32_t eot_raw28, eot_raw2c, eot_raw30; /* partial task fields */
    uint64_t raw60; /* only for independent source/destination mappings */
    uint32_t fragment_usc[A04_VENDOR_COPY_USC_BYTES/4];
    uint32_t frozen_task[52];
    uint32_t service_expected[1024];
    BOOLEAN service_expected_active;
    BOOLEAN prepared,exposed,usc_ready,task_frozen;
};
NTSTATUS a04_vendor_copy_scratch_prepare(struct a04_vendor_copy_scratch *owner,
    struct a04_vendor_render_cold *render,ULONG map_registers);
NTSTATUS a04_vendor_copy_scratch_mark_exposed(struct a04_vendor_copy_scratch *owner);
/* Trusted kernel input only; cold owner, exact fixed native asset and no GPU
 * exposure. Successful staging is not a command submission authorization. */
NTSTATUS a04_vendor_copy_scratch_stage_usc(struct a04_vendor_copy_scratch *owner,
    const void *asset,size_t bytes);
/* CPU-only fixed-profile evidence image. Each bit describes one derived
 * DWORD; even a full mask does not authorize submission or prove pixels. */
struct a04_vendor_copy_task_snapshot { uint32_t words[52];uint64_t known_words; };
NTSTATUS a04_vendor_copy_scratch_task_snapshot(struct a04_vendor_copy_scratch *owner,
    struct a04_vendor_copy_task_snapshot *snapshot);
/* Freeze before firmware start; exposure and retention are one-way. */
NTSTATUS a04_vendor_copy_scratch_freeze(struct a04_vendor_copy_scratch *owner);
/* Experiment only: caller holds the device lifetime/power serialization for
 * the entire call and keeps the owner, adapter and mappings through reboot. */
NTSTATUS a04_vendor_render_copy_once(struct a04_vendor_copy_scratch *owner,
    struct a04_vendor_start *start,struct a04_vendor_clear_result *result);
/* A caller holds the device lifetime lock; all IO is staged in private DMA.
 * After timeout the owner is poisoned and remains retained. */
NTSTATUS a04_vendor_render_copy_buffer(struct a04_vendor_copy_scratch *owner,
    struct a04_vendor_start *start,const void *input,void *output,size_t bytes);
NTSTATUS a04_vendor_copy_scratch_release(struct a04_vendor_copy_scratch *owner);
#endif
