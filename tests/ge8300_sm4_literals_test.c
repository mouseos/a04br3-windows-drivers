/* SPDX-License-Identifier: MIT. Real bounded decoder, no GPU success claim. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/compiler/sm4.h"
#include "../drivers/gpu/mt8167/wddm-render/clear_protocol.h"
static void output(uint32_t *t,uint32_t n){t[0]=0x40;t[1]=n;t[2]=0x3000065;t[3]=0x1020f2;t[4]=0;}
int main(void){
 struct geo_sm2_program p;struct geo_sm4_signatures sig={0};struct geo_sm2_error err;
 uint32_t t[128]={0},n,i;sig.output_count=1;sig.outputs[0].mask=15;
 /* Real SM4 immediate32 scalar broadcasts to all four destination lanes. */
 output(t,11);t[5]=0x5000036;t[6]=0x1020f2;t[7]=0;t[8]=0x4001;t[9]=0x3e800000;t[10]=0x100003e;
 assert(!geo_sm4_decode(&p,1,t,44,&sig,&err));
 assert(p.count==2&&p.instructions[0].opcode==81&&p.instructions[1].opcode==1);
 assert(p.constant_mask==0&&p.immediate_mask==0x80000000u);
 for(i=0;i<4;i++)assert(p.instructions[0].immediate[i]==0x3e800000);
 assert(p.instructions[1].src[0].index==31&&p.instructions[1].src[0].swizzle==0xe4);
 /* Same vector bits are interned, including NaN payload/signed zero. */
 output(t,19);
 t[5]=0x0d000000;t[6]=0x1020f2;t[7]=0;
 for(i=0;i<2;i++){unsigned o=8+i*5;t[o]=0x4002;t[o+1]=0;t[o+2]=0x80000000;t[o+3]=0x3f800000;t[o+4]=0x7fc12345;}
 t[18]=0x100003e;t[1]=19;
 assert(!geo_sm4_decode(&p,1,t,76,&sig,&err));assert(p.count==2&&p.instructions[1].opcode==2);
 assert(p.instructions[1].src[0].index==p.instructions[1].src[1].index);
 assert(p.instructions[0].immediate[1]==0x80000000&&p.instructions[0].immediate[3]==0x7fc12345);
 /* Runtime CB and literal never alias; literal is not a shared register. */
 output(t,21);t[5]=0x4000059;t[6]=0x208e46;t[7]=0;t[8]=1;
 t[9]=0x0b000000;t[10]=0x1020f2;t[11]=0;t[12]=0x208e46;t[13]=0;t[14]=0;
 t[15]=0x4002;t[16]=0x3f800000;t[17]=0;t[18]=0;t[19]=0;t[20]=0x100003e;t[1]=21;
 assert(!geo_sm4_decode(&p,1,t,84,&sig,&err));assert(p.constant_mask==1&&p.instructions[0].dst.index==31);
 t[8]=32;assert(geo_sm4_decode(&p,1,t,84,&sig,&err)!=GEO_OK);t[8]=1;
 /* Out-of-range cb31 must not borrow an existing literal slot31. */
 t[12]=0x4002;t[13]=0;t[14]=0;t[15]=0;t[16]=0;t[17]=0x208e46;t[18]=0;t[19]=31;
 assert(geo_sm4_decode(&p,1,t,84,&sig,&err)!=GEO_OK);
 /* Every truncation rejects. Index dimensions/extended/64-bit reject. */
 output(t,14);t[5]=0x8000036;t[6]=0x1020f2;t[7]=0;t[8]=0x4002;t[9]=1;t[10]=2;t[11]=3;t[12]=4;t[13]=0x100003e;
 assert(!geo_sm4_decode(&p,1,t,56,&sig,&err));
 for(n=0;n<14;n++){t[1]=n;assert(geo_sm4_decode(&p,1,t,n*4,&sig,&err)!=GEO_OK);}t[1]=14;
 for(i=0;i<3;i++){const uint32_t bad[]={0x104002,0x80004002,0x5002};t[8]=bad[i];assert(geo_sm4_decode(&p,1,t,56,&sig,&err)!=GEO_OK);}
 puts("SM4 scalar/vector IEEE literals, dedup, cb disjointness, bounds and unsupported forms PASS");return 0;
}
