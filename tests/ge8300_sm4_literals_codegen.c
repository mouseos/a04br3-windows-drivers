/* SPDX-License-Identifier: MIT. Real source->NIR->PCO, no hardware access. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/compiler/shader_compiler.h"
#include "../drivers/gpu/mt8167/wddm-render/clear_protocol.h"
int main(void){
 uint32_t t[]={0x40,14,0x3000065,0x1020f2,0,0x8000036,0x1020f2,0,0x4002,0x3f800000,0,0,0x3f800000,0x100003e};
 struct geo_sm4_signatures s={0};struct geo_shader_compiler c;struct geo_compiled_shader a,b;struct geo_sm2_error e;void *owner=NULL;
 s.output_count=1;s.outputs[0].mask=15;
 assert(!geo_pco_device_create(UINT64_C(0x001600280036001e),&c,&owner));
 assert(!geo_pco_compile_tokenized(c.context,1,t,sizeof(t),&s,&a,&e));
 assert(a.code&&a.code_bytes&&!a.constant_mask&&!a.shareds);
 t[9]=0;t[10]=0x3f800000;
 assert(!geo_pco_compile_tokenized(c.context,1,t,sizeof(t),&s,&b,&e));
 assert(b.code&&b.code_bytes&&!b.constant_mask&&!b.shareds);
 assert(a.code_bytes!=b.code_bytes||memcmp(a.code,b.code,a.code_bytes));
 printf("PCO literal red/green distinct code: %u/%u bytes, shareds0 constant_mask0; GPU untested\n",a.code_bytes,b.code_bytes);
 c.release(c.context,&a);c.release(c.context,&b);geo_pco_device_destroy(&c,owner);return 0;
}
