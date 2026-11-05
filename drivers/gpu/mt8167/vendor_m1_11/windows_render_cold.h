/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_WINDOWS_RENDER_COLD_H
#define A04_VENDOR_WINDOWS_RENDER_COLD_H
#include "windows_stage.h"
#include "windows_uncached.h"
#include "render_resources.h"
struct a04_vendor_render_cold {
    struct a04_vendor_uncached buffer[5];
    struct a04_vendor_render_layout layout;
    struct a04_vendor_m1_11_memory *firmware;
    uint32_t client_write[4], sequence;
    BOOLEAN mapped, poisoned;
};
/* Firmware is staged but has never been started. Each buffer remains pinned
 * after processor release until a separately verified DMA-quiescence path. */
NTSTATUS a04_vendor_render_prepare_cold(struct a04_vendor_render_cold *render,
    struct a04_vendor_m1_11_memory *firmware, PDMA_ADAPTER adapter);
NTSTATUS a04_vendor_render_release_cold(struct a04_vendor_render_cold *render);
struct a04_vendor_start;
/* GP UPDATE or a TA/3D/TQ priority change; no drawing task is submitted. */
NTSTATUS a04_vendor_render_context_command(struct a04_vendor_render_cold *render,
    struct a04_vendor_start *start, unsigned index, uint32_t priority);
#endif
