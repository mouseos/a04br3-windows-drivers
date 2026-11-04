/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_WINDOWS_LINEAR_CONTEXT_H
#define A04_GE8300_WINDOWS_LINEAR_CONTEXT_H
#include "windows_linear_transfer.h"
#include "linear_copy_fixture.h"

/* Owns an isolated fixed-copy GPU address space and its firmware queue.
 * Zero-initialize before start. The adapter, firmware, context and all DMA
 * allocations remain live until firmware has stopped and DMA is quiescent.
 * Start submits a zero-filled initial copy; success includes its fence and
 * queue read completion. Only PASSIVE_LEVEL, with firmware commands serialized.
 * On failure after allocation the owner still calls free only after GPU stop.
 * This is a fixed-command staging context, not a WDDM or SG implementation. */
struct a04_windows_linear_context {
    struct a04_gpu_firmware_state *firmware;
    struct { PDMA_ADAPTER adapter; void *cpu; PHYSICAL_ADDRESS logical; } memory;
    struct a04_gpu_fw_object vm;
    struct a04_gpu_fw_queue queue;
    struct rogue_fwif_cmd_transfer command;
    struct a04_windows_transfer_mapping mapping;
};
NTSTATUS a04_windows_linear_context_start(struct a04_windows_linear_context *context,
                                         struct a04_gpu_firmware_state *firmware);
NTSTATUS a04_windows_linear_context_free(struct a04_windows_linear_context *context,
                                        struct a04_gpu_firmware_state *firmware);
/* Copy exactly 16 KiB via the two private common-buffer staging areas.
 * Both input and output are trusted, resident kernel buffers, valid throughout
 * the call. They may overlap each other but not the context's own DMA or the
 * firmware heap. Caller serializes the context with all firmware commands.
 * On failure, output is untouched and context resources remain retained until
 * firmware stop if DMA might still be active. This is not an SG mapping or a
 * framebuffer presentation interface. */
NTSTATUS a04_windows_linear_context_copy(struct a04_windows_linear_context *context,
                                         const void *input,void *output);
#endif
