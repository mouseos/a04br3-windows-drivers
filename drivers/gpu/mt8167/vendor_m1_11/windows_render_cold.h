/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_WINDOWS_RENDER_COLD_H
#define A04_VENDOR_WINDOWS_RENDER_COLD_H
#include "windows_stage.h"
#include "windows_uncached.h"
#include "render_resources.h"
#include "clear_image.h"
struct a04_vendor_render_cold {
    struct a04_vendor_uncached buffer[16];
    struct a04_vendor_clear_image clear;
    struct a04_vendor_render_layout layout;
    struct a04_vendor_m1_11_memory *firmware;
    uint32_t client_write[4], sequence;
    BOOLEAN mapped, poisoned, clear_ready, clear_attempted;
};
/* Firmware is staged but has never been started. Each buffer remains pinned
 * after processor release until a separately verified DMA-quiescence path. */
NTSTATUS a04_vendor_render_prepare_cold(struct a04_vendor_render_cold *render,
    struct a04_vendor_m1_11_memory *firmware, PDMA_ADAPTER adapter, BOOLEAN prepare_clear);
NTSTATUS a04_vendor_render_release_cold(struct a04_vendor_render_cold *render);
struct a04_vendor_start;
/* GP UPDATE or a TA/3D/TQ priority change; no drawing task is submitted. */
NTSTATUS a04_vendor_render_context_command(struct a04_vendor_render_cold *render,
    struct a04_vendor_start *start, unsigned index, uint32_t priority);
struct a04_vendor_clear_result {
    uint32_t zeros, guard_errors, first, last, phase;
    uint32_t kread,kwrite,cread,cwrite,ufo,expected;
};
/* One bounded owned 32x32 transfer, followed by SLC flush and CPU readback. */
NTSTATUS a04_vendor_render_clear_once(struct a04_vendor_render_cold *render,
    struct a04_vendor_start *start, struct a04_vendor_clear_result *result);
#endif
