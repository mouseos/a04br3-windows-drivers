/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_WINDOWS_RENDER_COLD_H
#define A04_VENDOR_WINDOWS_RENDER_COLD_H
#include "windows_stage.h"
#include "windows_uncached.h"
#include "render_resources.h"
#include "clear_image.h"
#include "windows_surface.h"
struct a04_vendor_render_cold {
    struct a04_vendor_uncached buffer[16];
    struct a04_vendor_clear_image clear;
    struct a04_vendor_render_layout layout;
    struct a04_vendor_m1_11_memory *firmware;
    struct a04_vendor_surface *screen_surface;
    uint32_t client_write[4], sequence, cleanup_mask;
    uint32_t clear_first, clear_row_bytes, diagnostic_tile_index;
    BOOLEAN mapped, poisoned, clear_ready, clear_attempted, clear_verified, cleanup_started;
};
/* Firmware is staged but has never been started. Each buffer remains pinned
 * after processor release until a separately verified DMA-quiescence path. */
NTSTATUS a04_vendor_render_prepare_cold(struct a04_vendor_render_cold *render,
    struct a04_vendor_m1_11_memory *firmware, PDMA_ADAPTER adapter, BOOLEAN prepare_clear, uint32_t clear_colour);
NTSTATUS a04_vendor_render_attach_screen(struct a04_vendor_render_cold *render,
    struct a04_vendor_surface *surface);
NTSTATUS a04_vendor_render_release_cold(struct a04_vendor_render_cold *render);
struct a04_vendor_start;
/* GP UPDATE or a TA/3D/TQ priority change; no drawing task is submitted. */
NTSTATUS a04_vendor_render_context_command(struct a04_vendor_render_cold *render,
    struct a04_vendor_start *start, unsigned index, uint32_t priority);
/* FW cleanup only; does not release mappings or establish fabric quiescence. */
NTSTATUS a04_vendor_render_cleanup_contexts(struct a04_vendor_render_cold *render,
    struct a04_vendor_start *start, unsigned *failed_index);
struct a04_vendor_clear_result {
    uint32_t zeros, guard_errors, first, last, phase; /* zeros = pixels matching requested value */
    uint32_t kread,kwrite,cread,cwrite,ufo,expected;
    uint32_t provisional,slc_status,slc_idle;
};
/* One bounded owned 32x32 transfer, followed by SLC flush and CPU readback. */
NTSTATUS a04_vendor_render_clear_once(struct a04_vendor_render_cold *render,
    struct a04_vendor_start *start, struct a04_vendor_clear_result *result);
/* Diagnostic-only copy into separately owned, nonpaged normal memory.
 * Caller owns a distinct allocation, guarantees no physical aliases with any
 * device/control memory, retains all objects and serializes their teardown.
 * Virtual overlap checks do not establish physical disjointness. No pointer
 * from dxgkrnl/scanout may be passed. Not a present callback. */
NTSTATUS a04_vendor_render_copy_verified_screen(struct a04_vendor_render_cold *render,
    struct a04_vendor_start *start, void *destination, size_t bytes);
/* Diagnostic-only fixed sequence; completed pixels do not prove DMA quiescence. */
NTSTATUS a04_vendor_render_next_tile(struct a04_vendor_render_cold *render,
    struct a04_vendor_start *start, unsigned index, struct a04_vendor_clear_result *result);
#endif
