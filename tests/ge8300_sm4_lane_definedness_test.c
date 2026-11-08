/* SPDX-License-Identifier: MIT
 * Real bounded SM4 decoder; no PCO/GPU generation or runtime success claim. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/compiler/sm4.h"
#include "../drivers/gpu/mt8167/wddm-render/clear_protocol.h"
struct code {uint32_t t[128];unsigned n;};
static uint32_t dst(unsigned type,unsigned mask){return 0x00100002u|(type<<12)|(mask<<4);}
static uint32_t src(unsigned type,unsigned swizzle){return 0x00100006u|(type<<12)|(swizzle<<4);}
static void put(struct code *c,uint32_t v){assert(c->n<128);c->t[c->n++]=v;}
static void begin(struct code *c,unsigned written){
 memset(c,0,sizeof(*c));put(c,0x10040);put(c,0);
 put(c,0x0300005f);put(c,dst(1,15));put(c,0);
 put(c,0x04000067);put(c,dst(2,15));put(c,0);put(c,1);
 put(c,0x02000068);put(c,1);
 put(c,0x05000036);put(c,dst(0,written));put(c,0);put(c,src(1,0xe4));put(c,0);
}
static void mov(struct code *c,unsigned mask,unsigned swizzle){
 put(c,0x05000036);put(c,dst(2,mask));put(c,0);put(c,src(0,swizzle));put(c,0);
}
static void fill_zw(struct code *c){put(c,0x05000036);put(c,dst(2,12));put(c,0);put(c,src(1,0xe4));put(c,0);}
static int decode(struct code *c,struct geo_sm2_program *p){
 struct geo_sm4_signatures s={0};struct geo_sm2_error error;
 put(c,0x0100003e);c->t[1]=c->n;
 s.input_count=1;s.output_count=1;s.inputs[0].mask=15;s.outputs[0].mask=15;s.outputs[0].system_value=1;
 return geo_sm4_decode(p,0,c->t,c->n*4,&s,&error);
}
static void dot(struct code *c,unsigned op){
 put(c,(7u<<24)|op);put(c,dst(2,15));put(c,0);put(c,src(0,0xe4));put(c,0);put(c,src(1,0xe4));put(c,0);
}
int main(void){struct code c;struct geo_sm2_program p;int r;
 begin(&c,3);mov(&c,3,0xe4);fill_zw(&c);r=decode(&c,&p);
#ifdef GEO_EXPECT_OLD_UNUSED_LANE_REJECTION
 assert(r==GEO_INVALID);
#else
 assert(r==GEO_OK && p.count==3 && p.instructions[1].dst.mask==3);
#endif
 begin(&c,7);dot(&c,16);r=decode(&c,&p);
#ifdef GEO_EXPECT_OLD_UNUSED_LANE_REJECTION
 assert(r==GEO_INVALID);
#else
 assert(r==GEO_OK && p.instructions[1].opcode==8);
#endif
 begin(&c,7);dot(&c,17);assert(decode(&c,&p)==GEO_INVALID); /* DP4 really consumes W. */
 begin(&c,3);dot(&c,16);assert(decode(&c,&p)==GEO_INVALID); /* DP3 really consumes Z. */
 begin(&c,1);mov(&c,15,0xaa);assert(decode(&c,&p)==GEO_INVALID); /* read undefined ZZZZ. */
 begin(&c,1);mov(&c,15,0);assert(decode(&c,&p)==GEO_OK); /* scalar XXXX replication. */
 begin(&c,15);mov(&c,15,0xe4);assert(decode(&c,&p)==GEO_OK); /* full defined vector. */
 begin(&c,15);mov(&c,3,0xe4);assert(decode(&c,&p)==GEO_INVALID); /* partial output still rejected. */
 begin(&c,15);unsigned at=c.n;mov(&c,15,0xe4);c.t[at]=0x05000045u;assert(decode(&c,&p)==GEO_UNSUPPORTED); /* SAMPLE remains unsupported. */
#ifdef GEO_EXPECT_OLD_UNUSED_LANE_REJECTION
 puts("SM4 source lane regression: prior unused-lane false rejections reproduced; actual uninitialized reads and SAMPLE rejected");
#else
 puts("SM4 source lane regression PASS: masked MOV/DP3 accepted; undefined live lanes/DP4-W/incomplete output/SAMPLE rejected; compiler decoder only");
#endif
 return 0;}
