/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* One focused host state/lifetime check, NOT GPU/WDK/interrupt validation. */
#include <assert.h>
#include <string.h>
#include "../clear_engine.h"
static unsigned publishes;
static int32_t create(void *c,const struct geo_surface_desc *d,uint64_t *s){(void)c;(void)d;*s=1;return GEO_OK;}
static int32_t bind(void *c,uint64_t s,void *m,uint32_t o,uint32_t n){(void)c;(void)s;(void)m;(void)o;(void)n;return GEO_OK;}
static int32_t release(void *c,uint64_t s){(void)c;(void)s;return GEO_OK;}
static int32_t prepare(void *c,uint64_t s,uint32_t color,uint64_t *job){(void)c;(void)s;(void)color;*job=7;return GEO_OK;}
static int32_t submit(void *c,uint64_t j,uint32_t f,uint32_t g){(void)c;(void)j;(void)f;(void)g;publishes++;return GEO_OK;}
int main(void) {
 struct geo_engine e;struct geo_allocation a={0};struct geo_dma_ticket t,old;
 struct geo_surface_desc d={sizeof(d),1,256,256,1024,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};
 struct geo_clear_packet p={GEO_CLEAR_MAGIC,sizeof(p),1,0,0xff00ff00,0,{0,0}};
 struct geo_owner_ops ops={sizeof(ops),1,GEO_OWNER_REQUIRED,0,&e,create,bind,release,release,prepare,release,submit};
 uint32_t fence=0;uint64_t token=0;
 assert(geo_engine_init(&e,&ops)==GEO_OK);assert(geo_create(&e,&a,&d)==GEO_OK);
 assert(geo_bind(&e,&a,&a,0,256*1024)==GEO_OK);
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
 /* GPU done alone cannot release pages or manufacture CPU visibility. */
 assert(geo_complete(&e,token,t.generation,GEO_OK,GEO_COMPLETION_GPU_DONE,&fence)==GEO_POISONED);
 assert(e.poisoned&&a.references==1&&fence==10);
 assert(geo_unbind(&e,&a)==GEO_POISONED);assert(geo_destroy(&e,&a)==GEO_POISONED);
 return 0;
}
