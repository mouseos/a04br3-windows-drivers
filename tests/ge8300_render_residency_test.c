/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/clear_engine.h"
static int creates,binds,unbinds,discards,submits;
static int32_t create(void*c,const struct geo_surface_desc*d,uint64_t*t){(void)c;(void)d;*t=++creates;return 0;}
static int32_t bind(void*c,uint64_t t,void*m,uint32_t o,uint32_t b,uint64_t a){(void)c;(void)t;(void)m;(void)o;(void)b;(void)a;binds++;return 0;}
static int32_t unbind(void*c,uint64_t t){(void)c;(void)t;unbinds++;return 0;}
static int32_t destroy(void*c,uint64_t t){(void)c;(void)t;return 0;}
static int32_t prepare(void*c,uint64_t t,uint32_t color,uint64_t*j){(void)c;(void)t;(void)color;*j=99;return 0;}
static int32_t discard(void*c,uint64_t t){(void)c;assert(t==99||t==100);discards++;return 0;}
static int32_t submit(void*c,uint64_t t,uint32_t f,uint32_t g){(void)c;assert(t==100&&f==7&&g);submits++;return 0;}
static int32_t prepare_copy_unused(void *c,uint64_t s,uint64_t d,uint64_t *j){(void)c;(void)s;(void)d;(void)j;return GEO_UNSUPPORTED;}
int main(void){
 struct geo_engine e;struct geo_allocation a={0};struct geo_dma_ticket t,t2;uint64_t token;uint32_t fence;
 struct geo_surface_desc d={sizeof(d),1,800,600,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};
 struct geo_clear_packet p={GEO_CLEAR_MAGIC,sizeof(p),1,0,0xffff00ff,0,{0,0}};
 struct geo_owner_ops o={sizeof(o),GEO_OWNER_OPS_VERSION,GEO_OWNER_REQUIRED,0,&e,create,bind,unbind,destroy,prepare,discard,submit,prepare_copy_unused};
 assert(geo_engine_init(&e,&o)==0&&geo_create(&e,&a,&d)==0);
 /* A real Render reservation may precede the initial VidMm map. */
 assert(geo_prepare_begin(&e,&a,&p,&t)==0&&!a.bound&&a.references==1);
 assert(geo_destroy(&e,&a)==GEO_BUSY);
 assert(geo_bind(&e,&a,&a,0,1920000,0)==0&&a.bound);
 assert(prepare(&e,a.surface,p.color,&token)==0&&geo_prepare_finish(&e,&t,0,token)==0);
 assert(geo_unbind(&e,&a)==GEO_BUSY);
 /* Relocation first really discards preparation; it retains the packet reference. */
 assert(geo_cancel_begin(&e,&t,&token)==0&&discard(&e,token)==0&&geo_reprepare_finish(&e,&t,0)==0);
 assert(a.references==1&&geo_unbind(&e,&a)==0&&!a.bound);
 assert(geo_bind(&e,&a,&a,0,1920000,4096)==0&&geo_prepare_finish(&e,&t,0,100)==0);
 assert(geo_submit(&e,&t,7)==0&&submits==1&&geo_unbind(&e,&a)==GEO_BUSY);
 assert(geo_complete(&e,100,t.generation,0,GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE,&fence)==0&&fence==7&&a.references==0);
 /* Cancelling an unprepared packet never asks the native owner to discard token zero. */
 assert(geo_prepare_begin(&e,&a,&p,&t2)==0&&geo_cancel_begin(&e,&t2,&token)==0&&token==0&&a.references==0);
 assert(geo_unbind(&e,&a)==0&&geo_destroy(&e,&a)==0);
 assert(binds==2&&unbinds==2&&discards==1&&e.allocation_count==0);
 puts("Render-before-residency, real reprepare/relocation, completion and pending cancellation passed");return 0;
}
