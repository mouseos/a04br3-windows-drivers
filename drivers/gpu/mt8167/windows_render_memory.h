/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_RENDER_MEMORY_H
#define A04_WINDOWS_RENDER_MEMORY_H
#include "windows_firmware.h"
#include "firmware_objects.h"
#include "render_context.h"
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
    struct a04_gpu_fw_object vm;
    struct a04_gpu_render_context render;
    BOOLEAN render_prepared;
};
NTSTATUS a04_windows_render_memory_create(struct a04_windows_render_memory *m,
    struct a04_gpu_firmware_state *s,ULONG payload_pages,
    const struct a04_windows_render_map *maps,size_t count);
/* Bind the private page-table root to a firmware memory-context object from
 * this firmware's arena. Caller serializes access to firmware runtime.next_free.
 * Once allocated the object is retained until firmware shutdown. */
NTSTATUS a04_windows_render_memory_bind_vm(struct a04_windows_render_memory *m);
/* Prepare firmware render objects using the bound VM and an owned writable
 * eight-byte VDM callstack. Static context registers and all their indirect
 * dependencies remain the caller's responsibility; this does not submit. */
NTSTATUS a04_windows_render_prepare_context(struct a04_windows_render_memory *m,
    uint32_t context_id,uint64_t callstack_gpu,unsigned isp_store_count,
    const struct rogue_fwif_geom_registers_caswitch *static_state);
/* Synchronize render objects and publish the VM; never submits a kick. */
NTSTATUS a04_windows_render_publish_context(struct a04_windows_render_memory *m);
/* Clean all pages and the bound firmware VM object, then wait for MMU
 * invalidation. Binding does not submit a render context or kick. On failure after the
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
