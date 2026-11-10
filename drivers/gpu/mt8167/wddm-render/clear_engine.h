/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_CLEAR_ENGINE_H
#define GEO_CLEAR_ENGINE_H
#include "owner_abi.h"
#include "shared_surface.h"
#define GEO_MAX_JOBS 32u
struct geo_allocation {
 struct geo_allocation *next;
 struct geo_surface_desc desc;
 uint32_t logical_width,logical_height;
 uint64_t surface;
 uint32_t references,bound,poisoned,open_count,cpu_only;
 uint64_t aperture_offset;uint32_t aperture_pages;
 uint32_t primary,source_id,display_refs;
 struct geo_shared_surface shared;
};
struct geo_job {
 struct geo_allocation *allocation,*vertex;
 void *context,*open_handle,*vertex_open_handle;
 uint32_t kind;
 struct geo_copy_region copy_region;
 uint64_t owner_job;
 uint32_t generation,state,fence,retired_generation;
 struct geo_allocation *texture;void *texture_open_handle;
};
struct geo_engine {
 struct geo_owner_ops owner;
 struct geo_allocation *allocations;
 uint32_t allocation_count;
 struct geo_job jobs[GEO_MAX_JOBS];
 uint32_t poisoned,submitted_ever,last_submitted,completed_ever,last_completed;
 uint32_t preempt_pending,preempt_fence,preempt_ready;
};
/* External serialization is mandatory. PASSIVE calls use an owner rundown;
 * submit/completion use the same interrupt-synchronized nonpaged state.
 * State changes do not wait or invent completion/drain. Owner PASSIVE
 * callbacks may prepare real allocations; publish/completion must not wait. */
#ifdef __cplusplus
extern "C" {
#endif
int32_t geo_engine_init(struct geo_engine *,const struct geo_owner_ops *);
int32_t geo_create(struct geo_engine *,struct geo_allocation *,const struct geo_surface_desc *);
int32_t geo_bind(struct geo_engine *,struct geo_allocation *,void *mdl,uint32_t offset,uint32_t bytes,uint64_t aperture_offset);
int32_t geo_unbind(struct geo_engine *,struct geo_allocation *);
int32_t geo_open_reference(struct geo_engine *,struct geo_allocation *);
int32_t geo_close_reference(struct geo_engine *,struct geo_allocation *,void *open_handle);
int32_t geo_destroy(struct geo_engine *,struct geo_allocation *);
int32_t geo_prepare_begin(struct geo_engine *,struct geo_allocation *,const struct geo_clear_packet *,struct geo_dma_ticket *);
int32_t geo_prepare_copy_region_begin(struct geo_engine *,struct geo_allocation *,struct geo_allocation *,const struct geo_copy_region *,struct geo_dma_ticket *);
int32_t geo_prepare_copy_begin(struct geo_engine *,struct geo_allocation *destination,struct geo_allocation *source,struct geo_dma_ticket *);
int32_t geo_prepare_draw_begin(struct geo_engine *,struct geo_allocation *target,struct geo_allocation *vertex,struct geo_dma_ticket *);
int32_t geo_prepare_texture_begin(struct geo_engine *,struct geo_allocation *,struct geo_allocation *,struct geo_allocation *,struct geo_dma_ticket *);
int32_t geo_prepare_finish(struct geo_engine *,const struct geo_dma_ticket *,int32_t status,uint64_t owner_job);
int32_t geo_cancel_begin(struct geo_engine *,const struct geo_dma_ticket *,uint64_t *owner_job);
int32_t geo_reprepare_finish(struct geo_engine *,const struct geo_dma_ticket *,int32_t status);
int32_t geo_cancel_finish(struct geo_engine *,const struct geo_dma_ticket *,int32_t status);
int32_t geo_request_preempt(struct geo_engine *,uint32_t fence);
int32_t geo_take_preemption(struct geo_engine *,uint32_t *preempt_fence,uint32_t *last_completed);
int32_t geo_cancel_prepared(struct geo_engine *,const struct geo_dma_ticket *);
int32_t geo_submit(struct geo_engine *,const struct geo_dma_ticket *,uint32_t fence);
/* Accept a captured packet without touching hardware. State7 is queued for
 * PASSIVE preparation; state8 is the single claimed current DMA buffer.
 * Claim/finish are externally interrupt-synchronized; preparation is outside
 * that lock. A claimed buffer finishes at the existing DMA preemption boundary. */
int32_t geo_submit_captured(struct geo_engine *,const struct geo_dma_ticket *,uint32_t fence);
int geo_submitted_prepare_ready(const struct geo_engine *);
int32_t geo_claim_submitted_prepare(struct geo_engine *,struct geo_dma_ticket *);
int32_t geo_finish_submitted_prepare(struct geo_engine *,const struct geo_dma_ticket *,int32_t status,uint64_t owner_job);
/* Only after actual owner drain/reset proof; never reports aborted work complete. */
int32_t geo_retire_after_owner_drain(struct geo_engine *);
int32_t geo_complete(struct geo_engine *,uint64_t owner_job,uint32_t generation,int32_t status,uint32_t visibility,uint32_t *fence);
#ifdef __cplusplus
}
#endif
#endif
