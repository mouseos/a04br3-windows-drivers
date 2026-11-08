/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* One focused host state/lifetime check, NOT GPU/WDK/interrupt validation. */
#include <assert.h>
#include <string.h>
#include "../clear_engine.h"
static unsigned publishes,objects;
static int32_t create(void *c,const struct geo_surface_desc *d,uint64_t *s){(void)c;(void)d;*s=++objects;return GEO_OK;}
static int32_t bind(void *c,uint64_t s,void *m,uint32_t o,uint32_t n,uint64_t aperture){(void)c;(void)s;(void)m;(void)o;(void)n;(void)aperture;return GEO_OK;}
static int32_t release(void *c,uint64_t s){(void)c;(void)s;return GEO_OK;}
static int32_t prepare(void *c,uint64_t s,uint32_t color,uint64_t *job){(void)c;(void)s;(void)color;*job=7;return GEO_OK;}
static int32_t submit(void *c,uint64_t j,uint32_t f,uint32_t g){(void)c;(void)j;(void)f;(void)g;publishes++;return GEO_OK;}
static int32_t prepare_copy_unused(void *c,uint64_t s,uint64_t d,uint64_t *j){(void)c;(void)s;(void)d;(void)j;return GEO_UNSUPPORTED;}
int main(void) {
 struct geo_engine e;struct geo_allocation a={0};struct geo_dma_ticket t,old,queued;
 struct geo_surface_desc d={sizeof(d),1,256,256,1024,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};
 struct geo_clear_packet p={GEO_CLEAR_MAGIC,sizeof(p),1,0,0xff00ff00,0,{0,0}};
 struct geo_owner_ops ops={sizeof(ops),GEO_OWNER_OPS_VERSION,GEO_OWNER_REQUIRED,0,&e,create,bind,release,release,prepare,release,submit,prepare_copy_unused};
 uint32_t fence=0,preempt=0,last=0;uint64_t token=0,cancel_token=0;
 assert(geo_engine_init(&e,&ops)==GEO_OK);assert(geo_create(&e,&a,&d)==GEO_OK);
 assert(geo_bind(&e,&a,&a,0,256*1024,0)==GEO_OK);
 p.reserved[0]=1;assert(geo_prepare_begin(&e,&a,&p,&t)==GEO_INVALID);p.reserved[0]=0;
 assert(geo_prepare_begin(&e,&a,&p,&t)==GEO_OK);old=t;
 assert(geo_unbind(&e,&a)==GEO_BUSY);assert(geo_destroy(&e,&a)==GEO_BUSY);
 assert(prepare(&e,a.surface,p.color,&token)==GEO_OK);
 assert(geo_prepare_finish(&e,&t,GEO_OK,token)==GEO_OK);
 assert(geo_submit(&e,&t,10)==GEO_OK);assert(publishes==1);
 assert(geo_complete(&e,token,t.generation+1,GEO_OK,3,&fence)==GEO_NOT_FOUND);
 assert(a.references==1&&fence==0);
 assert(geo_complete(&e,token,t.generation,GEO_OK,3,&fence)==GEO_OK);assert(fence==10&&a.references==0);
 assert(geo_prepare_begin(&e,&a,&p,&t)==GEO_OK);assert(t.generation!=old.generation);
 assert(geo_submit(&e,&old,11)==GEO_NOT_FOUND);
 assert(geo_prepare_finish(&e,&t,GEO_OK,token)==GEO_OK);assert(geo_submit(&e,&t,11)==GEO_OK);
 /* A second accepted packet stays software-queued, not hardware-published. */
 assert(geo_prepare_begin(&e,&a,&p,&queued)==GEO_OK);
 assert(geo_prepare_finish(&e,&queued,GEO_OK,8)==GEO_OK);
 assert(geo_submit(&e,&queued,12)==GEO_OK);assert(publishes==2);
 assert(geo_request_preempt(&e,33)==GEO_OK);
 assert(geo_take_preemption(&e,&preempt,&last)==GEO_NOT_READY);
 assert(geo_complete(&e,token,t.generation,GEO_OK,3,&fence)==GEO_OK);
 assert(fence==11&&publishes==2); /* queued packet must not start past preempt */
 assert(geo_take_preemption(&e,&preempt,&last)==GEO_OK);assert(preempt==33&&last==11);
 assert(a.references==1&&geo_unbind(&e,&a)==GEO_BUSY);
 assert(geo_submit(&e,&queued,13)==GEO_OK);assert(publishes==3);
 /* GPU done alone cannot release pages or manufacture CPU visibility. */
 assert(geo_complete(&e,8,queued.generation,GEO_OK,GEO_COMPLETION_GPU_DONE,&fence)==GEO_POISONED);
 assert(e.poisoned&&a.references==1&&fence==11);
 assert(geo_unbind(&e,&a)==GEO_POISONED);assert(geo_destroy(&e,&a)==GEO_POISONED);
 old=queued;assert(geo_retire_after_owner_drain(&e)==GEO_OK);
 assert(!e.poisoned&&a.references==0&&e.last_completed==11&&e.last_submitted==13);
 assert(geo_complete(&e,8,old.generation,GEO_OK,3,&fence)==GEO_NOT_FOUND);
 /* Cancel reserves an unpublished token across PASSIVE owner discard. */
 assert(geo_prepare_begin(&e,&a,&p,&t)==GEO_OK);assert(geo_prepare_finish(&e,&t,GEO_OK,9)==GEO_OK);
 assert(geo_cancel_begin(&e,&t,&cancel_token)==GEO_OK&&cancel_token==9);
 assert(geo_submit(&e,&t,14)==GEO_BUSY);assert(release(&e,cancel_token)==GEO_OK);
 assert(geo_cancel_finish(&e,&t,GEO_OK)==GEO_OK);assert(a.references==0&&e.last_completed==11);
 /* One shared scheduler generation retains both actual allocation roles. */
 {
  struct geo_allocation v={0};struct geo_surface_desc vd={sizeof(vd),1,48,1,48,GEO_FORMAT_VERTEXDATA,GEO_USAGE_VERTEX_BUFFER,0};
  assert(geo_create(&e,&v,&vd)==GEO_OK);assert(geo_bind(&e,&v,&v,0,48,0x40000)==GEO_OK);
  assert(geo_prepare_draw_begin(&e,&a,&v,&t)==GEO_OK);assert(a.references==1&&v.references==1);
  assert(geo_prepare_finish(&e,&t,GEO_OK,10)==GEO_OK);assert(geo_submit(&e,&t,14)==GEO_OK);
  assert(geo_complete(&e,10,t.generation+1,GEO_OK,3,&fence)==GEO_NOT_FOUND);assert(a.references==1&&v.references==1);
  assert(geo_complete(&e,10,t.generation,GEO_OK,3,&fence)==GEO_OK);assert(a.references==0&&v.references==0&&fence==14);
  assert(geo_prepare_draw_begin(&e,&a,&v,&t)==GEO_OK);
  assert(geo_prepare_finish(&e,&t,GEO_BUSY,11)==GEO_BUSY);assert(a.references==1&&v.references==1&&e.poisoned);
  assert(geo_retire_after_owner_drain(&e)==GEO_OK);assert(a.references==0&&v.references==0);
  assert(geo_unbind(&e,&v)==GEO_OK);assert(geo_destroy(&e,&v)==GEO_OK);
 }
 assert(geo_unbind(&e,&a)==GEO_OK);assert(geo_destroy(&e,&a)==GEO_OK);
 assert(e.allocation_count==0&&e.allocations==0);
 return 0;
}
