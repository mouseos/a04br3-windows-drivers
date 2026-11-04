/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_RENDER_MEMORY_H
#define A04_WINDOWS_RENDER_MEMORY_H
#include "windows_firmware.h"
#if A04_GE8300_RENDER_EXPERIMENT
/* A private DMA allocation and its GPU page tables. No command bytes are
 * accepted here. Call at PASSIVE_LEVEL; the caller serializes firmware access. */
#define A04_RENDER_MAX_MAPS 32u
#define A04_RENDER_PAYLOAD_OFFSET (14u*4096u)
struct a04_windows_render_map {
    uint64_t gpu_va;
    uint32_t payload_page, pages, group;
    uint64_t mmu_flags;
};
struct a04_windows_render_memory {
    struct a04_gpu_firmware_state *firmware;
    PDMA_ADAPTER adapter;
    void *cpu;
    PHYSICAL_ADDRESS logical;
    ULONG length;
    BOOLEAN ready, published, poisoned;
    size_t map_count;
    struct a04_windows_render_map maps[A04_RENDER_MAX_MAPS];
};
NTSTATUS a04_windows_render_memory_create(struct a04_windows_render_memory *m,
    struct a04_gpu_firmware_state *s,ULONG payload_pages,
    const struct a04_windows_render_map *maps,size_t count);
/* Clean all pages and wait for MMU invalidation. On any failure after the
 * attempt begins keep the allocation and mark it poisoned. */
NTSTATUS a04_windows_render_memory_publish(struct a04_windows_render_memory *m);
/* Stage only the generated fragment PDS into owned, read-only GPU mappings
 * before publication. This does not supply or validate USC code or submit a
 * job. shader_gpu is the start and entry of the fixed 360-byte fragment USC
 * binary; its contents remain the caller's responsibility. Callers serialize
 * memory access and retain the DMA allocation. */
NTSTATUS a04_windows_render_stage_fragment_pds(struct a04_windows_render_memory *m,
    uint64_t pds_gpu,uint64_t shader_gpu,uint64_t doutu_control);
/* gpu_dma_quiesced is an independently established GPU/DMA stop, not a
 * fence alone. A false value retains memory even if the processor is stopped. */
NTSTATUS a04_windows_render_memory_free(struct a04_windows_render_memory *m, BOOLEAN gpu_dma_quiesced);
#endif
#endif
