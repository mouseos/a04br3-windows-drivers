/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Production engine FIFO/preemption/reference contract; no WDK or GPU proof. */
#include <assert.h>
#include <stdio.h>
#include "../clear_engine.h"
static unsigned ids,published,claimed_irq;
static uint32_t last_fence;
static int32_t create(void *c,const struct geo_surface_desc *d,uint64_t *s){(void)c;(void)d;*s=++ids;return GEO_OK;}
static int32_t bind(void *c,uint64_t s,void *m,uint32_t o,uint32_t n,uint64_t a){(void)c;(void)s;(void)m;(void)o;(void)n;(void)a;return GEO_OK;}
static int32_t release(void *c,uint64_t s){(void)c;(void)s;return GEO_OK;}
static int32_t prepare(void *c,uint64_t s,uint32_t color,uint64_t *j){(void)c;(void)s;(void)color;(void)j;assert(!"engine must not prepare at enqueue");return GEO_UNSUPPORTED;}
static int32_t copy(void *c,uint64_t s,uint64_t d,uint64_t *j){(void)c;(void)s;(void)d;(void)j;assert(!"engine must not prepare at enqueue");return GEO_UNSUPPORTED;}
static int32_t submit(void *c,uint64_t j,uint32_t f,uint32_t g){(void)c;assert(j&&g);if(claimed_irq)return GEO_SUBMIT_WAIT_IRQ;published++;last_fence=f;return GEO_OK;}
static const struct geo_surface_desc rt={sizeof(rt),1,800,600,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};
static const struct geo_surface_desc vb={sizeof(vb),1,48,1,48,GEO_FORMAT_VERTEXDATA,GEO_USAGE_VERTEX_BUFFER,0};
static const struct geo_surface_desc st={sizeof(st),1,800,600,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_STAGING,0};
static const struct geo_clear_packet clear={GEO_CLEAR_MAGIC,sizeof(clear),1,0,0xffff00ff,0,{0,0}};
static void init(struct geo_engine *e,struct geo_allocation *a){
 struct geo_owner_ops o={sizeof(o),GEO_OWNER_OPS_VERSION,GEO_OWNER_REQUIRED,0,e,create,bind,release,release,prepare,release,submit,copy};
 published=0;last_fence=0;claimed_irq=0;
 assert(geo_engine_init(e,&o)==GEO_OK);assert(geo_create(e,a,&rt)==GEO_OK);
 assert(geo_bind(e,a,a,0,1920000,0)==GEO_OK);
}
int main(void){
 {
  struct geo_engine e;struct geo_allocation a={0},v={0},d={0};struct geo_dma_ticket c,t,q,head;
  uint32_t done=0;init(&e,&a);
  assert(geo_create(&e,&v,&vb)==GEO_OK&&geo_bind(&e,&v,&v,0,48,1921024)==GEO_OK);
  assert(geo_create(&e,&d,&st)==GEO_OK&&geo_bind(&e,&d,&d,0,1920000,1925120)==GEO_OK);
  assert(geo_prepare_begin(&e,&a,&clear,&c)==GEO_OK);
  assert(geo_prepare_draw_begin(&e,&a,&v,&t)==GEO_OK);
  assert(geo_prepare_copy_begin(&e,&d,&a,&q)==GEO_OK);
  assert(a.references==3&&v.references==1&&d.references==1);
  assert(geo_submit_captured(&e,&c,10)==GEO_OK);
  assert(geo_submit_captured(&e,&t,11)==GEO_OK);
  assert(geo_submit_captured(&e,&q,12)==GEO_OK&&published==0);
  assert(geo_unbind(&e,&a)==GEO_BUSY&&geo_submitted_prepare_ready(&e));
  assert(geo_claim_submitted_prepare(&e,&head)==GEO_OK&&head.slot==c.slot);
  assert(!geo_submitted_prepare_ready(&e));
  assert(geo_finish_submitted_prepare(&e,&head,GEO_OK,101)==GEO_OK&&published==1&&last_fence==10);
  assert(geo_claim_submitted_prepare(&e,&head)==GEO_NOT_READY);
  assert(geo_complete(&e,101,c.generation+1,GEO_OK,3,&done)==GEO_NOT_FOUND&&a.references==3);
  assert(geo_complete(&e,101,c.generation,GEO_OK,3,&done)==GEO_OK&&done==10&&published==1);
  assert(geo_claim_submitted_prepare(&e,&head)==GEO_OK&&head.slot==t.slot);
  assert(geo_finish_submitted_prepare(&e,&head,GEO_OK,102)==GEO_OK&&last_fence==11);
  assert(geo_complete(&e,102,t.generation,GEO_OK,3,&done)==GEO_OK&&done==11);
  assert(geo_claim_submitted_prepare(&e,&head)==GEO_OK&&head.slot==q.slot);
  assert(geo_finish_submitted_prepare(&e,&head,GEO_OK,103)==GEO_OK&&last_fence==12);
  assert(geo_complete(&e,103,q.generation,GEO_OK,3,&done)==GEO_OK&&done==12);
  assert(published==3&&!a.references&&!v.references&&!d.references);
 }
 {
  struct geo_engine e;struct geo_allocation a={0};struct geo_dma_ticket first,second,head;
  uint32_t pf=0,last=0,done=0;uint64_t unused=99;init(&e,&a);
  assert(geo_prepare_begin(&e,&a,&clear,&first)==GEO_OK&&geo_prepare_begin(&e,&a,&clear,&second)==GEO_OK);
  assert(geo_submit_captured(&e,&first,20)==GEO_OK&&geo_submit_captured(&e,&second,21)==GEO_OK);
  assert(geo_request_preempt(&e,90)==GEO_OK&&geo_take_preemption(&e,&pf,&last)==GEO_OK);
  assert(pf==90&&last==0&&published==0&&a.references==2);
  assert(e.jobs[first.slot].state==4&&e.jobs[second.slot].state==4);
  assert(geo_submit_captured(&e,&first,22)==GEO_OK&&geo_claim_submitted_prepare(&e,&head)==GEO_OK);
  assert(geo_request_preempt(&e,91)==GEO_OK&&geo_take_preemption(&e,&pf,&last)==GEO_NOT_READY);
  assert(geo_finish_submitted_prepare(&e,&head,GEO_OK,201)==GEO_OK&&published==1);
  assert(geo_take_preemption(&e,&pf,&last)==GEO_NOT_READY);
  assert(geo_complete(&e,201,first.generation,GEO_OK,3,&done)==GEO_OK&&done==22);
  assert(geo_take_preemption(&e,&pf,&last)==GEO_OK&&pf==91&&last==22);
  assert(geo_cancel_begin(&e,&second,&unused)==GEO_OK&&unused==0&&a.references==0);
 }
 {
  struct geo_engine e;struct geo_allocation a={0};struct geo_dma_ticket first,second,head;uint32_t done=0;
  init(&e,&a);assert(geo_prepare_begin(&e,&a,&clear,&first)==GEO_OK&&geo_prepare_begin(&e,&a,&clear,&second)==GEO_OK);
  assert(geo_submit_captured(&e,&first,30)==GEO_OK&&geo_submit_captured(&e,&second,31)==GEO_OK);
  assert(geo_claim_submitted_prepare(&e,&head)==GEO_OK);
  assert(geo_finish_submitted_prepare(&e,&head,GEO_UNSUPPORTED,0)==GEO_UNSUPPORTED);
  assert(e.poisoned&&published==0&&a.references==2&&!e.completed_ever);
  assert(geo_claim_submitted_prepare(&e,&head)==GEO_POISONED);
  assert(geo_complete(&e,999,first.generation,GEO_OK,3,&done)==GEO_NOT_FOUND&&done==0);
  /* The test caller supplies the external owner-drain model here; this API
   * does not itself prove that any physical device has stopped. */
  assert(geo_retire_after_owner_drain(&e)==GEO_OK&&a.references==0&&!e.completed_ever);
 }
 {
  /* A real startup IRQ can be claimed between PASSIVE preparation and the
   * atomic publish. Keep the current DMA pinned; service is not completion. */
  struct geo_engine e;struct geo_allocation a={0};struct geo_dma_ticket t,next;
  uint32_t done=0,pf=0,last=0;init(&e,&a);
  assert(geo_prepare_begin(&e,&a,&clear,&t)==GEO_OK);
  assert(geo_submit_captured(&e,&t,40)==GEO_OK&&geo_claim_submitted_prepare(&e,&t)==GEO_OK);
  claimed_irq=1;
  assert(geo_finish_submitted_prepare(&e,&t,GEO_OK,301)==GEO_SUBMIT_WAIT_IRQ);
  assert(e.jobs[t.slot].state==8&&e.jobs[t.slot].owner_job==301);
  assert(!e.poisoned&&a.references==1&&published==0&&!e.completed_ever);
  assert(geo_claim_submitted_prepare(&e,&next)==GEO_NOT_READY);
  assert(geo_unbind(&e,&a)==GEO_BUSY);
  assert(geo_finish_submitted_prepare(&e,&t,GEO_OK,302)==GEO_INVALID);
  assert(geo_complete(&e,301,t.generation,GEO_OK,3,&done)==GEO_NOT_FOUND);
  assert(geo_request_preempt(&e,99)==GEO_OK&&geo_take_preemption(&e,&pf,&last)==GEO_NOT_READY);
  claimed_irq=0; /* model receipt service; deliberately no DMA completion */
  assert(geo_finish_submitted_prepare(&e,&t,GEO_OK,301)==GEO_OK&&published==1&&last_fence==40);
  assert(a.references==1&&!e.completed_ever&&geo_take_preemption(&e,&pf,&last)==GEO_NOT_READY);
  assert(geo_complete(&e,301,t.generation,GEO_OK,3,&done)==GEO_OK&&done==40);
  assert(geo_take_preemption(&e,&pf,&last)==GEO_OK&&pf==99&&last==40);
  assert(a.references==0);
 }
 puts("FIFO/preemption/reference/startup-IRQ host model passed; no GPU/WDK proof");return 0;
}
