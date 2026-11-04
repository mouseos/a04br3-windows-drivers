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
#include "render_fixed_vdm.h"
#include "render_fixed_ppp.h"
#include "render_context_static.h"
#if A04_GE8300_RENDER_EXPERIMENT
/* A private DMA allocation and its GPU page tables. Call at PASSIVE_LEVEL;
 * the caller serializes firmware access and prevents untracked CPU writes. */
#define A04_RENDER_MAX_MAPS 32u
#define A04_RENDER_MAX_STAGED 64u
#define A04_RENDER_MAX_WRITABLE 16u
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
    BOOLEAN completion_observed,bounded_states_checked;
    BOOLEAN fence_snapshot_valid;
    uint32_t geom_fence_snapshot,frag_fence_snapshot,geom_read_snapshot,frag_read_snapshot;
    uint32_t geom_end_expected,frag_end_expected,frag_fence_expected;
    struct { uint64_t gpu_va;uint32_t blocks;BOOLEAN valid; } bounded_states[2];
    size_t bounded_state_count;
    BOOLEAN kick_prepared,kick_submitted,kick_partial;
    struct rogue_fwif_kccb_cmd kick,final_kick;
    /* Optional diagnostic; candidate scan is not a GPU execution gate. */
    BOOLEAN trace_capture_enabled,trace_capture_valid,trace_capture_changed;
    NTSTATUS trace_capture_status;
    struct a04_shared_trace_summary trace_summary;
    struct { uint64_t gpu_va,size; } staged[A04_RENDER_MAX_STAGED];
    size_t staged_count;
    /* Context and HWRT writable dependencies remain reserved until the
     * whole allocation is freed after independently established DMA stop. */
    struct { uint64_t gpu_va,size; } writable[A04_RENDER_MAX_WRITABLE];
    size_t writable_count;
    struct { uint64_t state,store_data,store_code,resume_data,resume_code; } pt_slots[2];
    struct { uint64_t state,store_shader,load_shader,store_data,store_code,load_data,load_code;BOOLEAN bounded; } sr_slots[2];
    size_t pt_slot_count,sr_slot_count;
    BOOLEAN bounded_static_prepared;
    struct rogue_fwif_geom_registers_caswitch bounded_static_state;
    BOOLEAN triangle_usc_staged;
    uint64_t triangle_vertex_usc,triangle_fragment_usc;
    BOOLEAN triangle_vertex_pds_staged,triangle_vertex_code_staged,triangle_fragment_pds_staged,triangle_uniform_staged;
    uint64_t triangle_vertex_pds,triangle_descriptor_pds,triangle_fragment_pds,triangle_vertex_spill;
    uint64_t triangle_vertex_spill_info,triangle_vertex_code,triangle_descriptor_code;
    uint64_t triangle_uniform,triangle_uniform_code,triangle_fragment_spill,triangle_fragment_spill_info,triangle_fs_meta;
    BOOLEAN static_heap_programs_staged;
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
/* Prepare both kicks and all ring commands before publishing. The first
 * fragment is partial; final_frag is queued by the second kick. */
NTSTATUS a04_windows_render_prepare_partial_pair(struct a04_windows_render_memory *m,
    const struct rogue_fwif_cmd_geom *geom,const struct rogue_fwif_cmd_frag *partial_frag,
    const struct rogue_fwif_cmd_frag *final_frag,uint32_t job_id);
/* Submit the prepared pair exactly once after publication. For partial work,
 * submit the final-fragment kick second. Any failure poisons the allocation;
 * no retries or releases without independent GPU/DMA quiescence. */
NTSTATUS a04_windows_render_submit_pair(struct a04_windows_render_memory *m);
/* Collect one submitted pair; for partial work require the final fence value.
 * Both fences and ring read offsets must reach their expected values before cleanup.
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
/* Stage the fixed probe's address-free PDS code into two owned read-only
 * allocations. This does not validate USC or authorize GPU execution. */
NTSTATUS a04_windows_render_stage_vertex_pds_code(struct a04_windows_render_memory *m,
    uint64_t vertex_code_gpu,uint64_t descriptor_code_gpu);
NTSTATUS a04_windows_render_stage_scratchless_pds(struct a04_windows_render_memory *m,
    const struct a04_render_pds_input *input,uint64_t vertex_gpu,uint64_t descriptor_gpu);
NTSTATUS a04_windows_render_stage_scratchless_pds_code(struct a04_windows_render_memory *m,
    uint64_t vertex_code_gpu,uint64_t descriptor_code_gpu);
/* Build a fixed 60-byte VDM stream. PPP contents and all remaining GPU
 * dependencies must be validated independently before any submission. */
NTSTATUS a04_windows_render_stage_fixed_vdm(struct a04_windows_render_memory *m,
    const struct a04_render_fixed_vdm_input *input,uint64_t vdm_gpu);
/* Stage fixed fragment uniform data, spill information, FS metadata and PDS
 * code atomically. PPP and shader contents still require validation. */
NTSTATUS a04_windows_render_stage_fragment_uniform(struct a04_windows_render_memory *m,
    const struct a04_render_fragment_uniform_input *input,uint64_t uniform_gpu,uint64_t code_gpu);
/* Stage both fixed PPP state blocks. Caller validates scissor, shader and all
 * other indirect references independently before allowing any GPU work. */
NTSTATUS a04_windows_render_stage_fixed_ppp(struct a04_windows_render_memory *m,
    const struct a04_render_fixed_ppp_input *input,uint64_t ppp_gpu,uint64_t finish_gpu);
/* One fixed 64x64 scissor entry; does not validate the fragment command. */
NTSTATUS a04_windows_render_stage_fixed_scissor(struct a04_windows_render_memory *m,uint64_t gpu);
/* Fixed no-texture probe: reserve the full Mesa-sized zero border table. */
NTSTATUS a04_windows_render_reserve_zero_border(struct a04_windows_render_memory *m,uint64_t gpu);
/* Firmware-fixed PDS VDM sync/EOT and USC VDM sync heap offsets. */
NTSTATUS a04_windows_render_stage_static_heap_programs(struct a04_windows_render_memory *m);
NTSTATUS a04_windows_render_verify_staged(struct a04_windows_render_memory *m,uint64_t gpu,
    size_t reserved,const uint8_t *expected,size_t used);
NTSTATUS a04_windows_render_verify_zero_target(struct a04_windows_render_memory *m,uint64_t gpu);
/* Stage fixed EOT DOUTU data. Shader provenance and PBE state remain
 * independent prerequisites for any GPU submission. */
NTSTATUS a04_windows_render_stage_pixel_event(struct a04_windows_render_memory *m,
    uint64_t pds_gpu,uint64_t shader_gpu);
/* Generate a fixed-layout EOT candidate and reserve its writable 16 KiB
 * target. This alone does not authorize submission or validate PBE bounds. */
NTSTATUS a04_windows_render_stage_fixed_eot(struct a04_windows_render_memory *m,
    uint64_t shader_gpu,uint64_t target_gpu);
/* Clear-only background constants and PDS DMA data; no texture descriptor. */
NTSTATUS a04_windows_render_stage_fixed_background(struct a04_windows_render_memory *m,
    uint64_t constants_gpu,uint64_t pds_gpu,uint64_t code_gpu);
NTSTATUS a04_windows_render_stage_background_shader(struct a04_windows_render_memory *m,
    uint64_t shader_gpu,uint64_t pds_gpu);
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
/* Bounded diagnostic USC is generated from a modified Mesa loop and remains
 * unverified on GPU. Both code and PDS must be staged before publication. */
NTSTATUS a04_windows_render_stage_bounded_context_usc(struct a04_windows_render_memory *m,
    uint64_t store_shader_gpu,uint64_t load_shader_gpu);
/* Fixed probe shaders; input is GPU VA only, with no captured BO bytes. */
NTSTATUS a04_windows_render_stage_triangle_usc(struct a04_windows_render_memory *m,
    uint64_t vertex_shader_gpu,uint64_t fragment_shader_gpu);
NTSTATUS a04_windows_render_stage_triangle_vertex_pds(struct a04_windows_render_memory *m,
    uint64_t spill_info_gpu,uint64_t spill_buffer_gpu,uint64_t vertex_pds_gpu,uint64_t descriptor_pds_gpu);
NTSTATUS a04_windows_render_stage_triangle_fragment_pds(struct a04_windows_render_memory *m,uint64_t pds_gpu);
NTSTATUS a04_windows_render_stage_triangle_vertex_code(struct a04_windows_render_memory *m,
    uint64_t vertex_code_gpu,uint64_t descriptor_code_gpu);
NTSTATUS a04_windows_render_stage_triangle_uniform(struct a04_windows_render_memory *m,
    uint64_t spill_info_gpu,uint64_t spill_buffer_gpu,uint64_t fs_meta_gpu,uint64_t data_gpu,uint64_t code_gpu);
NTSTATUS a04_windows_render_stage_bounded_context_sr(struct a04_windows_render_memory *m,
    uint64_t state_gpu,uint64_t store_shader_gpu,uint64_t load_shader_gpu,
    uint64_t store_data_gpu,uint64_t store_code_gpu,uint64_t load_data_gpu,uint64_t load_code_gpu);
/* Only after fence completion and CPU synchronization. A zero state does not
 * prove that the GPU performed a context switch. */
NTSTATUS a04_windows_render_check_bounded_states(struct a04_windows_render_memory *m);
/* Form static context registers only from previously staged bounded slots.
 * VDM and TA state ranges must be owned, zeroed and SLC-bypassed. */
NTSTATUS a04_windows_render_make_bounded_static(struct a04_windows_render_memory *m,
    const struct a04_render_context_static_input *input,struct rogue_fwif_geom_registers_caswitch *out);
/* gpu_dma_quiesced is an independently established GPU/DMA stop, not a
 * fence alone. A false value retains memory even if the processor is stopped. */
NTSTATUS a04_windows_render_memory_free(struct a04_windows_render_memory *m, BOOLEAN gpu_dma_quiesced);
#endif
#endif
