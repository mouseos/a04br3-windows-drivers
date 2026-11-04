/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_RENDER_MEMORY_H
#define A04_WINDOWS_RENDER_MEMORY_H
#include "windows_firmware.h"
#include "firmware_objects.h"
#include "render_context.h"
#include "render_target.h"
#include "pvr_rogue_fwif_client.h"
#include "render_trace_shared.h"
#include "render_pds_data.h"
#if A04_GE8300_RENDER_EXPERIMENT
/* A private DMA allocation and its GPU page tables. Call at PASSIVE_LEVEL;
 * the caller serializes firmware access and prevents untracked CPU writes. */
#define A04_RENDER_MAX_MAPS 32u
#define A04_RENDER_MAX_STAGED 32u
#define A04_RENDER_PAYLOAD_OFFSET (14u*4096u)
struct a04_windows_render_map {
    uint64_t gpu_va;
    uint32_t payload_page, pages, group;
    uint64_t mmu_flags;
};
struct a04_windows_render_freelist_layout {
    uint64_t table_gpu;
    uint32_t first_payload_page,page_count,id;
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
    struct a04_gpu_fw_object freelists[2];
    struct a04_windows_render_freelist_layout freelist_layout[2];
    BOOLEAN freelists_prepared;
    struct a04_gpu_render_target target;
    struct a04_render_hwrt_params target_params;
    BOOLEAN target_prepared;
    BOOLEAN cleanup_complete;
    BOOLEAN kick_prepared,kick_submitted;
    struct rogue_fwif_kccb_cmd kick;
    /* Optional diagnostic; candidate scan is not a GPU execution gate. */
    BOOLEAN trace_capture_enabled,trace_capture_valid,trace_capture_changed;
    NTSTATUS trace_capture_status;
    struct a04_shared_trace_summary trace_summary;
    struct { uint64_t gpu_va,size; } staged[A04_RENDER_MAX_STAGED];
    size_t staged_count;
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
/* Two non-growing PM lists backed solely by disjoint, unmapped payload pages.
 * Tables require PM/FW-protected, SLC-bypassed GPU mappings. Layout and
 * minimum sizes are diagnostic inputs, not hardware validation. */
NTSTATUS a04_windows_render_prepare_freelists(struct a04_windows_render_memory *m,
    const struct a04_windows_render_freelist_layout layouts[2]);
/* Prepare the fixed 64x64 SIPF1 target from disjoint owned writable ranges.
 * The small non-growing PM lists remain experimental and unverified on GPU. */
NTSTATUS a04_windows_render_prepare_target(struct a04_windows_render_memory *m,
    const struct a04_render_hwrt_params *params);
/* Synchronize render objects and publish the VM; never submits a kick. */
NTSTATUS a04_windows_render_publish_context(struct a04_windows_render_memory *m);
/* Trusted internal building block, not a fixture validator or a user-facing
 * entry point. Caller must independently establish every command dependency,
 * shader provenance and GPU access bound, and serialize all firmware access.
 * Prepare before publishing; publish synchronizes rings and page tables. */
NTSTATUS a04_windows_render_prepare_pair(struct a04_windows_render_memory *m,
    const struct rogue_fwif_cmd_geom *geom,const struct rogue_fwif_cmd_frag *frag,uint32_t job_id);
/* Submit the prepared non-partial pair exactly once after publication. Any
 * failure after the submission attempt poisons the allocation; no retries or
 * releases without independent GPU/DMA quiescence. */
NTSTATUS a04_windows_render_submit_pair(struct a04_windows_render_memory *m);
/* Collect one previously submitted, non-partial pair. Both fence values and
 * ring read offsets must reach their exact expected values before cleanup.
 * Serialized caller; this does not submit, retire the MMU, or free memory. */
NTSTATUS a04_windows_render_collect_pair(struct a04_windows_render_memory *m);
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
/* Stage vertex and descriptor PDS data and their two buffer-info records.
 * The caller remains responsible for shader provenance, all other GPU
 * references and execution authorization. No GPU work is submitted. */
NTSTATUS a04_windows_render_stage_vertex_pds(struct a04_windows_render_memory *m,
    const struct a04_render_pds_input *input,uint64_t vertex_gpu,uint64_t descriptor_gpu);
/* One context-switch slot's persistent PDS data and code. Inputs must name
 * unique owned mappings: writable state, read-only PDS output, no aliases.
 * Before publication only. Shared PDS/USC and kick remain separate. */
NTSTATUS a04_windows_render_stage_context_pt(struct a04_windows_render_memory *m,
    uint64_t state_gpu,uint64_t store_data_gpu,uint64_t store_code_gpu,
    uint64_t resume_data_gpu,uint64_t resume_code_gpu);
/* Stage one shared-register store/load PDS pair. The caller must independently
 * establish shader provenance, prevent overlapping future staging, and keep
 * all state and shader mappings alive. This does not authorize GPU work. */
NTSTATUS a04_windows_render_stage_context_sr(struct a04_windows_render_memory *m,
    uint64_t state_gpu,uint64_t store_shader_gpu,uint64_t load_shader_gpu,
    uint64_t store_control,uint64_t load_control,
    uint64_t store_data_gpu,uint64_t store_code_gpu,
    uint64_t load_data_gpu,uint64_t load_code_gpu);
/* gpu_dma_quiesced is an independently established GPU/DMA stop, not a
 * fence alone. A false value retains memory even if the processor is stopped. */
NTSTATUS a04_windows_render_memory_free(struct a04_windows_render_memory *m, BOOLEAN gpu_dma_quiesced);
#endif
#endif
