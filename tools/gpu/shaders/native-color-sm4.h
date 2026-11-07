/* SPDX-License-Identifier: MIT
 * Finite actual SM4.0 token decoder for the source COLOR4 pair. Operand
 * fields follow WDK d3d10TokenizedProgramFormat.hpp and H compiler/sm4.c.
 * This is not a generic SM4 compiler or a device command parser. */
#ifndef A04_NATIVE_COLOR_SM4_H
#define A04_NATIVE_COLOR_SM4_H
#include <stdint.h>
struct a04_color_moves { uint32_t position_input,color_input; };
/* Output is unchanged on failure. Return 0 success, 1 malformed, 2 unsupported.
 * The caller separately verifies real runtime signature/input-layout metadata. */
static int a04_decode_color_sm4(struct a04_color_moves *out,unsigned stage,
 const uint32_t *t,unsigned bytes)
{
 struct a04_color_moves made={0,0};unsigned at=2,inputs=0,outputs=0,moved=0,used=0;
 if(!out || !t || stage>1 || ((uintptr_t)t&3) || bytes<12 || bytes>256 || (bytes&3))return 1;
 unsigned words=bytes/4;
 if(t[0]!=((stage?0u:1u)<<16|0x40u))return 2;
 if(t[1]!=words)return 1;
 while(at<words) {
  unsigned start=at,token=t[at++],op=token&0x7ffu,len=(token>>24)&127u;
  if(!len || len>words-start)return 1;
  if(op==62) {
   if(token!=0x0100003eu || at!=words)return 1;
   if(inputs!=(stage?1u:3u) || outputs!=(stage?1u:3u) || moved!=(stage?1u:3u))return 2;
   *out=made;return 0;
  }
  if(op==95 || op==98 || op==101 || op==103) {
   unsigned declaration=stage?(op==98 || op==101):(op==95 || op==101 || op==103);
   if(!declaration || used || len!=(op==103?4u:3u))return 2;
   unsigned control=(stage && op==98)?0x1000u:0; /* LINEAR, perspective */
   if(token!=((len<<24)|control|op))return 2;
   unsigned operand=t[at++],reg=t[at++];
   if(reg>(stage?0u:1u))return 2;
   unsigned bit=1u<<reg;
   if(op==95 || op==98) {
    if(operand!=0x001010f2u || (inputs&bit))return 2;
    inputs|=bit;
   } else {
    if(operand!=0x001020f2u || (outputs&bit))return 2;
    if(!stage && ((reg==0)!=(op==103)))return 2;
    if(op==103 && t[at++]!=1u)return 2; /* SV_Position */
    outputs|=bit;
   }
  } else if(op==54) {
   if(token!=0x05000036u || len!=5)return 2; /* Full float4 MOV only. */
   unsigned dst=t[at++],d=t[at++],src=t[at++],s=t[at++];
   if(dst!=0x001020f2u || src!=0x00101e46u || d>(stage?0u:1u) || s>(stage?0u:1u) ||
      !(outputs&(1u<<d)) || !(inputs&(1u<<s)) || (moved&(1u<<d)))return 2;
   if(stage && s)return 2;
   if(!stage){if(!d)made.position_input=s;else made.color_input=s;}
   moved|=1u<<d;used=1;
  } else return 2;
  if(at!=start+len)return 1;
 }
 return 1;
}
#endif
