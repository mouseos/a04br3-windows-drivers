/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "clear_engine.h"
#include "memory.h"
#include "frontend/draw_abi.h"
static int serial_after(uint32_t a,uint32_t b) {
 uint32_t delta=a-b;return delta&&delta<0x80000000u;
}
int32_t geo_engine_init(struct geo_engine *e,const struct geo_owner_ops *o) {
 if(!e||!o||o->size!=sizeof(*o)||o->version!=GEO_OWNER_OPS_VERSION||o->reserved||
    (o->capabilities&GEO_OWNER_REQUIRED)!=GEO_OWNER_REQUIRED||!o->context||!o->create||!o->bind||
    !o->unbind||!o->destroy||!o->prepare||!o->discard_prepared||!o->submit||!o->prepare_copy||!o->prepare_copy_region)return GEO_UNSUPPORTED;
 memset(e,0,sizeof(*e));e->owner=*o;return GEO_OK;
}
int32_t geo_create(struct geo_engine *e,struct geo_allocation *a,const struct geo_surface_desc *d) {
 uint64_t token=0;int32_t s;struct geo_allocation *scan;
 if(!e||!a||!geo_desc_valid(d)||a->surface)return GEO_INVALID;
 if(e->poisoned)return GEO_POISONED;
 if(e->allocation_count==UINT32_MAX)return GEO_BUSY;
 s=e->owner.create(e->owner.context,d,&token);
 if(s!=GEO_OK)return s;
 if(!token){e->poisoned=1;return GEO_POISONED;}
 for(scan=e->allocations;scan;scan=scan->next)if(scan->surface==token){e->poisoned=1;return GEO_POISONED;}
 memset(a,0,sizeof(*a));a->desc=*d;a->logical_width=d->width;a->logical_height=d->height;a->surface=token;a->cpu_only=d->usage==GEO_USAGE_INDEX_BUFFER||d->usage==GEO_USAGE_STAGING||d->usage==GEO_USAGE_TEXTURE2D;
 a->next=e->allocations;e->allocations=a;e->allocation_count++;return GEO_OK;
}
/* Pending Render packets retain allocation objects, not physical residency.
 * Only packets with no native preparation may coexist with a bind/unbind. */
static int pending_only(const struct geo_engine *e,const struct geo_allocation *a) {
 uint32_t i,count=0;
 for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].state&&(e->jobs[i].allocation==a||e->jobs[i].vertex==a||e->jobs[i].texture==a)) {
  if(e->jobs[i].state!=4)return 0;
  count++;
 }
 return count==a->references;
}
int32_t geo_bind(struct geo_engine *e,struct geo_allocation *a,void *mdl,uint32_t off,uint32_t bytes,uint64_t aperture_offset) {
 int32_t s;
 if((aperture_offset&4095u)||aperture_offset>UINT64_MAX-(((uint64_t)bytes+4095u)&~UINT64_C(4095)))return GEO_INVALID;
 if(!e||!a||!a->surface||!mdl||a->bound||!pending_only(e,a)||off>UINT32_MAX-bytes||bytes!=(uint64_t)a->desc.pitch*a->desc.height)return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 s=e->owner.bind(e->owner.context,a->surface,mdl,off,bytes,aperture_offset);
 if(s!=GEO_OK){a->poisoned=1;e->poisoned=1;return s;}
 a->bound=1;return GEO_OK;
}
int32_t geo_unbind(struct geo_engine *e,struct geo_allocation *a) {
 int32_t s;
 if(!e||!a||!a->surface)return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 if(a->references&&!pending_only(e,a))return GEO_BUSY;
 if(!a->bound)return GEO_INVALID;
 s=e->owner.unbind(e->owner.context,a->surface);
 if(s!=GEO_OK){a->poisoned=1;e->poisoned=1;return s;}
 a->bound=0;return GEO_OK;
}
/* Caller holds frontend PASSIVE serialization; close additionally uses the
 * existing IRQ synchronizer because jobs can finish concurrently. No native
 * create/destroy/bind is performed on a per-process open/close. */
int32_t geo_open_reference(struct geo_engine *e,struct geo_allocation *a) {
 struct geo_allocation *scan;
 if(!e||!a)return GEO_INVALID;
 for(scan=e->allocations;scan&&scan!=a;scan=scan->next){}
 if(!scan)return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 if(a->open_count==UINT32_MAX)return GEO_BUSY;
 a->open_count++;return GEO_OK;
}
int32_t geo_close_reference(struct geo_engine *e,struct geo_allocation *a,void *handle) {
 struct geo_allocation *scan;uint32_t i;
 if(!e||!a||!handle)return GEO_INVALID;
 for(scan=e->allocations;scan&&scan!=a;scan=scan->next){}
 if(!scan||!a->open_count)return GEO_INVALID;
 for(i=0;i<GEO_MAX_JOBS;i++){
  const struct geo_job *j=&e->jobs[i];
  if(j->state&&j->state!=4&&(j->open_handle==handle||j->vertex_open_handle==handle||j->texture_open_handle==handle))return GEO_BUSY;
 }
 a->open_count--;return GEO_OK;
}
int32_t geo_destroy(struct geo_engine *e,struct geo_allocation *a) {
 int32_t s;struct geo_allocation **link;
 if(!e||!a||(!a->surface&&!a->cpu_only))return GEO_INVALID;
 for(link=&e->allocations;*link&&*link!=a;link=&(*link)->next){}
 if(!*link||!e->allocation_count)return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 if(a->bound||a->references||a->open_count)return GEO_BUSY;
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
 if(!e||!a||!t||!geo_packet_valid(p,1)||p->allocation_index||!geo_usage_renderable(a->desc.usage))return GEO_INVALID;
 if(e->poisoned||a->poisoned)return GEO_POISONED;
 if(!a->surface)return GEO_NOT_READY;
 for(i=0;i<GEO_MAX_JOBS;i++)if(!e->jobs[i].state&&e->jobs[i].generation!=UINT32_MAX)break;
 if(i==GEO_MAX_JOBS||a->references==UINT32_MAX)return GEO_BUSY;
 j=&e->jobs[i];j->generation++;j->allocation=a;j->vertex=0;j->vertex_open_handle=0;j->texture=0;j->texture_open_handle=0;j->kind=GEO_CLEAR_MAGIC;j->context=0;j->open_handle=0;j->owner_job=0;j->state=4;a->references++;
 t->magic=GEO_TICKET_MAGIC;t->slot=i;t->generation=j->generation;t->reserved=0;return GEO_OK;
}
int32_t geo_prepare_draw_begin(struct geo_engine *e,struct geo_allocation *a,struct geo_allocation *v,struct geo_dma_ticket *t) {
 uint32_t i;struct geo_job *j;
 if(!e||!a||!v||a==v||!t||!geo_usage_renderable(a->desc.usage)||v->desc.usage!=GEO_USAGE_VERTEX_BUFFER)return GEO_INVALID;
 if(e->poisoned||a->poisoned||v->poisoned)return GEO_POISONED;
 if(!a->surface||!v->surface)return GEO_NOT_READY;
 for(i=0;i<GEO_MAX_JOBS;i++)if(!e->jobs[i].state&&e->jobs[i].generation!=UINT32_MAX)break;
 if(i==GEO_MAX_JOBS||a->references==UINT32_MAX||v->references==UINT32_MAX)return GEO_BUSY;
 j=&e->jobs[i];j->generation++;j->allocation=a;j->vertex=v;j->kind=GEO_DRAW_MAGIC;
 j->context=0;j->open_handle=0;j->vertex_open_handle=0;j->texture=0;j->texture_open_handle=0;j->owner_job=0;j->state=4;
 a->references++;v->references++;
 t->magic=GEO_TICKET_MAGIC;t->slot=i;t->generation=j->generation;t->reserved=0;return GEO_OK;
}
int32_t geo_prepare_copy_region_begin(struct geo_engine *e,struct geo_allocation *d,struct geo_allocation *s,const struct geo_copy_region *region,struct geo_dma_ticket *t) {
 uint32_t i;struct geo_job *j;
 if(!e||!d||!s||d==s||!t||(!e->owner.prepare_copy||!e->owner.prepare_copy_region)||d->desc.usage!=GEO_USAGE_STAGING||!geo_usage_renderable(s->desc.usage)||
    d->desc.format!=GEO_FORMAT_A8R8G8B8||s->desc.format!=GEO_FORMAT_A8R8G8B8||
    !geo_copy_region_valid(region,s->logical_width,s->logical_height,d->logical_width,d->logical_height))return GEO_INVALID;
 if(e->poisoned||d->poisoned||s->poisoned)return GEO_POISONED;
 if(!d->surface||!s->surface)return GEO_NOT_READY;
 for(i=0;i<GEO_MAX_JOBS;i++)if(!e->jobs[i].state&&e->jobs[i].generation!=UINT32_MAX)break;
 if(i==GEO_MAX_JOBS||d->references==UINT32_MAX||s->references==UINT32_MAX)return GEO_BUSY;
 j=&e->jobs[i];j->generation++;j->allocation=d;j->vertex=s;j->texture=0;
 j->context=0;j->open_handle=0;j->vertex_open_handle=0;j->texture_open_handle=0;
 j->copy_region=*region;j->owner_job=0;j->kind=GEO_COPY_MAGIC;j->state=4;d->references++;s->references++;
 t->magic=GEO_TICKET_MAGIC;t->slot=i;t->generation=j->generation;t->reserved=0;return GEO_OK;
}
int32_t geo_prepare_copy_begin(struct geo_engine *e,struct geo_allocation *d,struct geo_allocation *s,struct geo_dma_ticket *t) {
 if(!s||!d||s->logical_width!=d->logical_width||s->logical_height!=d->logical_height)return GEO_INVALID;
 struct geo_copy_region r={0,0,0,0,s->logical_width,s->logical_height};
 return geo_prepare_copy_region_begin(e,d,s,&r,t);
}
int32_t geo_prepare_texture_begin(struct geo_engine *e,struct geo_allocation *a,struct geo_allocation *v,struct geo_allocation *x,struct geo_dma_ticket *t) {
 int32_t status;
 if(!e||!a||!v||!x||x==a||x==v||!geo_usage_sampleable(x->desc.usage))return GEO_INVALID;
 if(x->poisoned||e->poisoned)return GEO_POISONED;
 if(!x->surface)return GEO_NOT_READY;
 if(x->references==UINT32_MAX)return GEO_BUSY;
 status=geo_prepare_draw_begin(e,a,v,t);if(status)return status;
 struct geo_job *j=job(e,t);j->texture=x;x->references++;return GEO_OK;
}
static void retire_job(struct geo_job *j) {
 j->allocation->references--;if(j->vertex)j->vertex->references--;if(j->texture)j->texture->references--;
 j->retired_generation=j->generation;j->state=0;j->allocation=0;j->vertex=0;j->texture=0;j->texture_open_handle=0;
 j->context=0;j->open_handle=0;j->vertex_open_handle=0;j->owner_job=0;j->kind=0;
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
 retire_job(j);return GEO_OK;
}
static int active(const struct geo_engine *e) {
 uint32_t i;for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].state==2||e->jobs[i].state==8)return 1;return 0;
}
static void publish_next(struct geo_engine *e) {
 uint32_t i;struct geo_job *next=0;int32_t status;
 if(e->poisoned||e->preempt_pending||active(e))return;
 for(i=0;i<GEO_MAX_JOBS;i++)if((e->jobs[i].state==5||e->jobs[i].state==7)&&(!next||serial_after(next->fence,e->jobs[i].fence)))next=&e->jobs[i];
 if(!next)return;
 if(next->state==7)return; /* A later prepared packet cannot pass this head. */
 status=e->owner.submit(e->owner.context,next->owner_job,next->fence,next->generation);
 if(status!=GEO_OK){next->state=3;next->allocation->poisoned=1;e->poisoned=1;return;}
 next->state=2;
}
int32_t geo_submit(struct geo_engine *e,const struct geo_dma_ticket *t,uint32_t fence) {
 struct geo_job *j=job(e,t);
 if(!j)return GEO_NOT_FOUND;
 if(e->poisoned)return GEO_POISONED;
 if(j->state!=1)return GEO_BUSY;
 if(e->submitted_ever&&!serial_after(fence,e->last_submitted))return GEO_INVALID;
 /* Render already reserved a real prepared job and all resources. Accept
  * into this bounded owned FIFO, publish at most ONE native DMA pair. */
 j->state=5;j->fence=fence;e->last_submitted=fence;e->submitted_ever=1;
 publish_next(e);return e->poisoned?GEO_POISONED:GEO_OK;
}
int32_t geo_submit_captured(struct geo_engine *e,const struct geo_dma_ticket *t,uint32_t fence) {
 struct geo_job *j=job(e,t);
 if(!j)return GEO_NOT_FOUND;
 if(e->poisoned)return GEO_POISONED;
 if(j->state!=4)return GEO_BUSY;
 if(e->submitted_ever&&!serial_after(fence,e->last_submitted))return GEO_INVALID;
 j->state=7;j->fence=fence;e->last_submitted=fence;e->submitted_ever=1;
 return GEO_OK; /* Acceptance is not native preparation or GPU completion. */
}
int32_t geo_claim_submitted_prepare(struct geo_engine *e,struct geo_dma_ticket *t) {
 uint32_t i,slot=GEO_MAX_JOBS;
 if(!e||!t)return GEO_INVALID;
 if(e->poisoned)return GEO_POISONED;
 if(e->preempt_pending||active(e))return GEO_NOT_READY;
 for(i=0;i<GEO_MAX_JOBS;i++)if((e->jobs[i].state==5||e->jobs[i].state==7)&&
    (slot==GEO_MAX_JOBS||serial_after(e->jobs[slot].fence,e->jobs[i].fence)))slot=i;
 if(slot==GEO_MAX_JOBS||e->jobs[slot].state!=7)return GEO_NOT_READY;
 e->jobs[slot].state=8;
 t->magic=GEO_TICKET_MAGIC;t->slot=slot;t->generation=e->jobs[slot].generation;t->reserved=0;
 return GEO_OK;
}
int geo_submitted_prepare_ready(const struct geo_engine *e) {
 uint32_t i,slot=GEO_MAX_JOBS;
 if(!e||e->poisoned||e->preempt_pending||active(e))return 0;
 for(i=0;i<GEO_MAX_JOBS;i++)if((e->jobs[i].state==5||e->jobs[i].state==7)&&
    (slot==GEO_MAX_JOBS||serial_after(e->jobs[slot].fence,e->jobs[i].fence)))slot=i;
 return slot<GEO_MAX_JOBS&&e->jobs[slot].state==7;
}
int32_t geo_finish_submitted_prepare(struct geo_engine *e,const struct geo_dma_ticket *t,int32_t status,uint64_t token) {
 struct geo_job *j=job(e,t);
 if(!j||j->state!=8)return GEO_NOT_FOUND;
 if(j->owner_job && j->owner_job!=token)return GEO_INVALID;
 if(status!=GEO_OK||!token||e->poisoned) {
  j->owner_job=token;j->state=3;j->allocation->poisoned=1;e->poisoned=1;
  return status!=GEO_OK?status:GEO_POISONED;
 }
 j->owner_job=token;
 /* Claim made this the current DMA buffer before a possible preempt request.
  * Finish its real submission and acknowledge preemption only at its actual
  * IRQ completion. Unclaimed state7 buffers never start past that request. */
 status=e->owner.submit(e->owner.context,token,j->fence,j->generation);
 if(status==GEO_SUBMIT_WAIT_IRQ)return status; /* state8/token/fence/refs retained */
 if(status!=GEO_OK){j->state=3;j->allocation->poisoned=1;e->poisoned=1;return status;}
 j->state=2;return GEO_OK;
}
int32_t geo_request_preempt(struct geo_engine *e,uint32_t fence) {
 if(!e)return GEO_INVALID;
 if(e->preempt_pending)return e->preempt_fence==fence?GEO_OK:GEO_BUSY;
 e->preempt_pending=1;e->preempt_fence=fence;
 /* A poisoned owner cannot supply even an idle proof; keep request pending
  * for real completion or OS TDR. Never manufacture DMA_PREEMPTED. */
 if(!e->poisoned&&!active(e))e->preempt_ready=1;
 return GEO_OK;
}
int32_t geo_take_preemption(struct geo_engine *e,uint32_t *fence,uint32_t *last) {
 uint32_t i;
 if(!e||!fence||!last)return GEO_INVALID;
 if(e->poisoned||!e->preempt_pending||!e->preempt_ready||active(e))return GEO_NOT_READY;
 /* These packets were accepted into software but NEVER hardware-published.
  * Retain tokens/pages for scheduler resubmit with a new nonpaging fence. */
 for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].state==5){e->jobs[i].state=1;e->jobs[i].fence=0;}
 for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].state==7){e->jobs[i].state=4;e->jobs[i].fence=0;}
 *fence=e->preempt_fence;*last=e->completed_ever?e->last_completed:0;
 e->preempt_pending=0;e->preempt_ready=0;return GEO_OK;
}
int32_t geo_cancel_begin(struct geo_engine *e,const struct geo_dma_ticket *t,uint64_t *token) {
 struct geo_job *j=job(e,t);
 if(!j||!token)return GEO_NOT_FOUND;
 if(e->poisoned)return GEO_POISONED;
 if(j->state==4){*token=0;retire_job(j);return GEO_OK;} /* No native resources exist yet. */
 if(j->state!=1)return GEO_BUSY;
 j->state=6;*token=j->owner_job;return GEO_OK;
}
int32_t geo_reprepare_finish(struct geo_engine *e,const struct geo_dma_ticket *t,int32_t status) {
 struct geo_job *j=job(e,t);
 if(!j||j->state!=6)return GEO_NOT_FOUND;
 if(status!=GEO_OK){j->state=3;j->allocation->poisoned=1;e->poisoned=1;return status;}
 j->owner_job=0;j->state=4;return GEO_OK;
}
int32_t geo_cancel_finish(struct geo_engine *e,const struct geo_dma_ticket *t,int32_t status) {
 struct geo_job *j=job(e,t);
 if(!j||j->state!=6)return GEO_NOT_FOUND;
 if(status!=GEO_OK){j->state=3;j->allocation->poisoned=1;e->poisoned=1;return status;}
 retire_job(j);return GEO_OK;
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
 for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].state==2&&serial_after(j->fence,e->jobs[i].fence)) {
  j->allocation->poisoned=1;j->state=3;e->poisoned=1;return GEO_POISONED;
 }
 *fence=j->fence;e->last_completed=j->fence;e->completed_ever=1;
 retire_job(j);
 if(e->preempt_pending)e->preempt_ready=1;else publish_next(e);
 return GEO_OK;
}

int32_t geo_retire_after_owner_drain(struct geo_engine *e) {
 struct geo_allocation *a;uint32_t i,count;
 if(!e)return GEO_INVALID;
 /* Validate all reference counts before changing any. External caller has
  * closed operation rundown and obtained real IRQ/DMA/token drain proof. */
 for(a=e->allocations;a;a=a->next) {
  count=0;for(i=0;i<GEO_MAX_JOBS;i++)if(e->jobs[i].state&&(e->jobs[i].allocation==a||e->jobs[i].vertex==a||e->jobs[i].texture==a))count++;
  if(a->references!=count){e->poisoned=1;return GEO_POISONED;}
 }
 for(i=0;i<GEO_MAX_JOBS;i++) {
  if(e->jobs[i].state)e->jobs[i].retired_generation=e->jobs[i].generation;
  e->jobs[i].vertex=0;e->jobs[i].vertex_open_handle=0;e->jobs[i].texture=0;e->jobs[i].texture_open_handle=0;e->jobs[i].kind=0;e->jobs[i].state=0;e->jobs[i].owner_job=0;e->jobs[i].allocation=0;e->jobs[i].context=0;e->jobs[i].open_handle=0;
  /* Keep generation: a delayed old token must never match a new job. */
 }
 for(a=e->allocations;a;a=a->next){a->references=0;a->poisoned=0;}
 e->poisoned=0;e->preempt_pending=0;e->preempt_ready=0;
 /* last_completed is unchanged: aborted jobs are NOT completed fences. */
 return GEO_OK;
}
