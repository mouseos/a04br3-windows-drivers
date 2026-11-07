/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "clear_engine.h"
#include "memory.h"
int32_t geo_engine_init(struct geo_engine *e,const struct geo_owner_ops *o) {
 if(!e||!o||o->size!=sizeof(*o)||o->version!=1||o->reserved||
    (o->capabilities&GEO_OWNER_REQUIRED)!=GEO_OWNER_REQUIRED||!o->context||!o->create||!o->bind||
    !o->unbind||!o->destroy||!o->prepare||!o->discard_prepared||!o->submit)return GEO_UNSUPPORTED;
 memset(e,0,sizeof(*e));e->owner=*o;return GEO_OK;
}
int32_t geo_create(struct geo_engine *e,struct geo_allocation *a,const struct geo_surface_desc *d) {
 uint64_t token=0;int32_t s;
 if(!e||!a||!geo_desc_valid(d)||a->surface)return GEO_INVALID;
 if(e->poisoned)return GEO_POISONED;
 if(e->allocation_count==UINT32_MAX)return GEO_BUSY;
 s=e->owner.create(e->owner.context,d,&token);
 if(s!=GEO_OK)return s;
 if(!token){e->poisoned=1;return GEO_POISONED;}
 memset(a,0,sizeof(*a));a->desc=*d;a->surface=token;
 a->next=e->allocations;e->allocations=a;e->allocation_count++;return GEO_OK;
}
int32_t geo_bind(struct geo_engine *e,struct geo_allocation *a,void *mdl,uint32_t off,uint32_t bytes) {
 int32_t s;
 if(!e||!a||!a->surface||!mdl||a->bound||a->references||off>UINT32_MAX-bytes||bytes!=(uint64_t)a->desc.pitch*a->desc.height)return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 s=e->owner.bind(e->owner.context,a->surface,mdl,off,bytes);
 if(s!=GEO_OK){a->poisoned=1;e->poisoned=1;return s;}
 a->bound=1;return GEO_OK;
}
int32_t geo_unbind(struct geo_engine *e,struct geo_allocation *a) {
 int32_t s;
 if(!e||!a||!a->surface)return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 if(a->references)return GEO_BUSY;
 if(!a->bound)return GEO_INVALID;
 s=e->owner.unbind(e->owner.context,a->surface);
 if(s!=GEO_OK){a->poisoned=1;e->poisoned=1;return s;}
 a->bound=0;return GEO_OK;
}
int32_t geo_destroy(struct geo_engine *e,struct geo_allocation *a) {
 int32_t s;struct geo_allocation **link;
 if(!e||!a||!a->surface)return GEO_INVALID;
 for(link=&e->allocations;*link&&*link!=a;link=&(*link)->next){}
 if(!*link||!e->allocation_count)return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 if(a->bound||a->references)return GEO_BUSY;
 s=e->owner.destroy(e->owner.context,a->surface);
 if(s!=GEO_OK){a->poisoned=1;e->poisoned=1;return s;}
 *link=a->next;e->allocation_count--;memset(a,0,sizeof(*a));return GEO_OK;
}
static struct geo_job *job(struct geo_engine *e,const struct geo_dma_ticket *t) {
 struct geo_job *j;
 if(!e||!t||t->magic!=GEO_TICKET_MAGIC||t->slot>=GEO_MAX_JOBS||t->reserved)return 0;
 j=&e->jobs[t->slot];return j->generation==t->generation&&j->state?j:0;
}
int32_t geo_prepare_begin(struct geo_engine *e,struct geo_allocation *a,const struct geo_clear_packet *p,struct geo_dma_ticket *t) {
 uint32_t i;struct geo_job *j;
 if(!e||!a||!t||!geo_packet_valid(p,1)||p->allocation_index||a->desc.usage!=GEO_USAGE_RENDER_TARGET)return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 if(!a->surface||!a->bound)return GEO_NOT_READY;
 for(i=0;i<GEO_MAX_JOBS;i++)if(!e->jobs[i].state&&e->jobs[i].generation!=UINT32_MAX)break;
 if(i==GEO_MAX_JOBS||a->references==UINT32_MAX)return GEO_BUSY;
 j=&e->jobs[i];j->generation++;j->allocation=a;j->owner_job=0;j->state=4;a->references++;
 t->magic=GEO_TICKET_MAGIC;t->slot=i;t->generation=j->generation;t->reserved=0;return GEO_OK;
}
int32_t geo_prepare_finish(struct geo_engine *e,const struct geo_dma_ticket *t,int32_t status,uint64_t token) {
 struct geo_job *j=job(e,t);
 if(!j||j->state!=4)return GEO_NOT_FOUND;
 if(status!=GEO_OK||!token||e->poisoned) {
  j->owner_job=token;j->state=3;j->allocation->poisoned=1;e->poisoned=1;
  return status!=GEO_OK?status:GEO_POISONED;
 }
 j->owner_job=token;j->state=1;return GEO_OK;
}
int32_t geo_cancel_prepared(struct geo_engine *e,const struct geo_dma_ticket *t) {
 struct geo_job *j=job(e,t);int32_t s;
 if(!j)return GEO_NOT_FOUND;
 if(e->poisoned)return GEO_POISONED;
 if(j->state!=1)return GEO_BUSY;
 s=e->owner.discard_prepared(e->owner.context,j->owner_job);
 if(s!=GEO_OK){j->allocation->poisoned=1;e->poisoned=1;return s;}
 j->allocation->references--;j->state=0;j->allocation=0;j->owner_job=0;return GEO_OK;
}
int32_t geo_submit(struct geo_engine *e,const struct geo_dma_ticket *t,uint32_t fence) {
 struct geo_job *j=job(e,t);int32_t s;
 if(!j)return GEO_NOT_FOUND;
 if(e->poisoned)return GEO_POISONED;
 if(j->state!=1)return GEO_BUSY;
 /* One scheduler engine: no wrap/reset until a real reset/quiescence contract exists. */
 if(e->submitted_ever&&fence<=e->last_submitted)return GEO_INVALID;
 s=e->owner.submit(e->owner.context,j->owner_job,fence,j->generation);
 if(s!=GEO_OK){j->allocation->poisoned=1;j->state=3;e->poisoned=1;return s;}
 j->state=2;j->fence=fence;e->last_submitted=fence;e->submitted_ever=1;return GEO_OK;
}
int32_t geo_complete(struct geo_engine *e,uint64_t token,uint32_t generation,int32_t status,uint32_t visibility,uint32_t *fence) {
 uint32_t i;struct geo_job *j;
 if(!e||!token||!fence)return GEO_INVALID;
 for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].owner_job==token&&e->jobs[i].generation==generation&&e->jobs[i].state==2)break;
 if(i==GEO_MAX_JOBS)return GEO_NOT_FOUND; /* stale completion cannot release a new generation */
 j=&e->jobs[i];
 if(e->poisoned)return GEO_POISONED;
 if(status!=GEO_OK||visibility!=(GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE)) {
  j->state=3;j->allocation->poisoned=1;e->poisoned=1;return status!=GEO_OK?status:GEO_POISONED;
 }
 /* Never advance past an earlier outstanding scheduler fence. */
 for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].state==2&&e->jobs[i].fence<j->fence) {
  j->allocation->poisoned=1;j->state=3;e->poisoned=1;return GEO_POISONED;
 }
 *fence=j->fence;e->last_completed=j->fence;e->completed_ever=1;
 j->allocation->references--;j->state=0;j->allocation=0;j->owner_job=0;return GEO_OK;
}

int32_t geo_retire_after_owner_drain(struct geo_engine *e) {
 struct geo_allocation *a;uint32_t i,count;
 if(!e)return GEO_INVALID;
 /* Validate all reference counts before changing any. External caller has
  * closed operation rundown and obtained real IRQ/DMA/token drain proof. */
 for(a=e->allocations;a;a=a->next) {
  count=0;for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].state&&e->jobs[i].allocation==a)count++;
  if(a->references!=count){e->poisoned=1;return GEO_POISONED;}
 }
 for(i=0;i<GEO_MAX_JOBS;i++) {
  e->jobs[i].state=0;e->jobs[i].owner_job=0;e->jobs[i].allocation=0;
  /* Keep generation: a delayed old token must never match a new job. */
 }
 for(a=e->allocations;a;a=a->next){a->references=0;a->poisoned=0;}
 e->poisoned=0;
 /* last_completed is unchanged: aborted jobs are NOT completed fences. */
 return GEO_OK;
}
