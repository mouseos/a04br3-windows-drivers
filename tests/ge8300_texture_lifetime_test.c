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
 struct geo_engine e;struct geo_allocation a={0},v={0},x={0};struct geo_dma_ticket t;uint64_t token;uint32_t fence;
 struct geo_surface_desc rt={sizeof(rt),1,800,600,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};
 struct geo_surface_desc vb={sizeof(vb),1,48,1,48,GEO_FORMAT_VERTEXDATA,GEO_USAGE_VERTEX_BUFFER,0};
 struct geo_surface_desc tx={sizeof(tx),2,4,4,16,GEO_FORMAT_A8R8G8B8,GEO_USAGE_TEXTURE2D,0};
 struct geo_owner_ops o={sizeof(o),GEO_OWNER_OPS_VERSION,GEO_OWNER_REQUIRED,0,&e,create,bind,unbind,destroy,prepare,discard,submit,prepare_copy_unused};
 assert(!geo_engine_init(&e,&o)&&!geo_create(&e,&a,&rt)&&!geo_create(&e,&v,&vb)&&!geo_create(&e,&x,&tx));
 assert(!geo_prepare_texture_begin(&e,&a,&v,&x,&t));
 assert(a.references==1&&v.references==1&&x.references==1&&geo_destroy(&e,&x)==GEO_BUSY);
 assert(!geo_bind(&e,&a,&a,0,1920000,0)&&!geo_bind(&e,&v,&v,0,48,469*4096)&&!geo_bind(&e,&x,&x,0,64,470*4096));
 assert(!geo_prepare_finish(&e,&t,0,99)&&geo_unbind(&e,&x)==GEO_BUSY);
 assert(!geo_cancel_begin(&e,&t,&token)&&token==99&&!discard(&e,token)&&!geo_reprepare_finish(&e,&t,0));
 assert(x.references==1&&!geo_unbind(&e,&x)&&!geo_bind(&e,&x,&x,0,64,471*4096));
 assert(!geo_prepare_finish(&e,&t,0,100)&&!geo_submit(&e,&t,7));
 assert(geo_unbind(&e,&a)==GEO_BUSY&&geo_unbind(&e,&v)==GEO_BUSY&&geo_unbind(&e,&x)==GEO_BUSY);
 assert(!geo_complete(&e,100,t.generation,0,GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE,&fence)&&fence==7);
 assert(!a.references&&!v.references&&!x.references);
 assert(!geo_unbind(&e,&x)&&!geo_unbind(&e,&v)&&!geo_unbind(&e,&a));
 assert(!geo_destroy(&e,&x)&&!geo_destroy(&e,&v)&&!geo_destroy(&e,&a)&&!e.allocation_count);
 puts("Texture third reference: Render before map, real discard/rebind, submitted-unmap refusal, completion/release passed");return 0;
}
