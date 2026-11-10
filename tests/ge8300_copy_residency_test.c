/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "clear_engine.h"
static uint64_t seq;
static int32_t create(void *c,const struct geo_surface_desc *d,uint64_t *t){(void)c;(void)d;*t=++seq;return 0;}
static int32_t bind(void*c,uint64_t s,void*m,uint32_t o,uint32_t b,uint64_t a){(void)c;(void)s;(void)m;(void)o;(void)b;(void)a;return 0;}
static int32_t done(void*c,uint64_t s){(void)c;(void)s;return 0;}
static int32_t prep(void*c,uint64_t s,uint32_t color,uint64_t*t){(void)c;(void)s;(void)color;*t=++seq;return 0;}
static int32_t submit(void*c,uint64_t t,uint32_t f,uint32_t g){(void)c;(void)t;(void)f;(void)g;return 0;}
static int32_t copy(void*c,uint64_t s,uint64_t d,uint64_t*t){(void)c;assert(s!=d);*t=++seq;return 0;}
static int32_t copy_region_owner(void*c,uint64_t s,uint64_t d,const struct geo_copy_region*r,uint64_t*j){if(!r||!r->width||!r->height)return GEO_INVALID;return copy(c,s,d,j);}
int main(void){
 struct geo_engine e;struct geo_owner_ops o={sizeof(o),GEO_OWNER_OPS_VERSION,GEO_OWNER_REQUIRED,0,&e,create,bind,done,done,prep,done,submit,copy,copy_region_owner};
 struct geo_allocation src={0},dst={0};struct geo_surface_desc d={sizeof(d),1,800,600,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};struct geo_dma_ticket t;uint32_t fence;
 assert(!geo_engine_init(&e,&o));assert(!geo_create(&e,&src,&d));d.usage=GEO_USAGE_STAGING;assert(!geo_create(&e,&dst,&d));
 assert(!geo_prepare_copy_begin(&e,&dst,&src,&t));assert(src.references==1&&dst.references==1&&!src.bound&&!dst.bound);
 assert(geo_destroy(&e,&src)==GEO_BUSY&&geo_destroy(&e,&dst)==GEO_BUSY);
 assert(!geo_bind(&e,&src,(void*)1,0,1920000,0));assert(!geo_bind(&e,&dst,(void*)2,0,1920000,1921024));
 assert(e.jobs[t.slot].kind==GEO_COPY_MAGIC&&e.jobs[t.slot].vertex==&src&&e.jobs[t.slot].allocation==&dst);
 assert(!geo_prepare_finish(&e,&t,0,99));assert(!geo_submit(&e,&t,1));
 assert(geo_unbind(&e,&src)==GEO_BUSY&&geo_unbind(&e,&dst)==GEO_BUSY);
 assert(!geo_complete(&e,99,t.generation,0,GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE,&fence)&&fence==1);
 assert(!src.references&&!dst.references);assert(!geo_unbind(&e,&src));assert(!geo_unbind(&e,&dst));
 assert(!geo_prepare_copy_begin(&e,&src,&dst,&t));
 assert(geo_destroy(&e,&src)==GEO_BUSY&&geo_destroy(&e,&dst)==GEO_BUSY);
 assert(!geo_bind(&e,&src,(void*)1,0,1920000,0));assert(!geo_bind(&e,&dst,(void*)2,0,1920000,1921024));
 assert(!geo_prepare_finish(&e,&t,0,100));assert(!geo_submit(&e,&t,2));
 assert(geo_unbind(&e,&src)==GEO_BUSY&&geo_unbind(&e,&dst)==GEO_BUSY);
 assert(!geo_complete(&e,100,t.generation,0,GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE,&fence)&&fence==2);
 assert(!geo_unbind(&e,&src));assert(!geo_unbind(&e,&dst));
 assert(!geo_destroy(&e,&src)&&!geo_destroy(&e,&dst));
 o.version=2;assert(geo_engine_init(&e,&o)==GEO_UNSUPPORTED);return 0;
}
