/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_CLEAR_ENGINE_H
#define GEO_CLEAR_ENGINE_H
#include "owner_abi.h"
#define GEO_MAX_JOBS 32u
struct geo_allocation {
 struct geo_surface_desc desc;
 uint64_t surface;
 uint32_t references,bound,poisoned;
 uint64_t aperture_offset;uint32_t aperture_pages;
};
struct geo_job {
 struct geo_allocation *allocation;
 uint64_t owner_job;
 uint32_t generation,state,fence;
};
struct geo_engine {
 struct geo_owner_ops owner;
 struct geo_job jobs[GEO_MAX_JOBS];
 uint32_t poisoned,submitted_ever,last_submitted,completed_ever,last_completed;
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
int32_t geo_bind(struct geo_engine *,struct geo_allocation *,void *mdl,uint32_t offset,uint32_t bytes);
int32_t geo_unbind(struct geo_engine *,struct geo_allocation *);
int32_t geo_destroy(struct geo_engine *,struct geo_allocation *);
int32_t geo_prepare_begin(struct geo_engine *,struct geo_allocation *,const struct geo_clear_packet *,struct geo_dma_ticket *);
int32_t geo_prepare_finish(struct geo_engine *,const struct geo_dma_ticket *,int32_t status,uint64_t owner_job);
int32_t geo_cancel_prepared(struct geo_engine *,const struct geo_dma_ticket *);
int32_t geo_submit(struct geo_engine *,const struct geo_dma_ticket *,uint32_t fence);
int32_t geo_complete(struct geo_engine *,uint64_t owner_job,uint32_t generation,int32_t status,uint32_t visibility,uint32_t *fence);
#ifdef __cplusplus
}
#endif
#endif
