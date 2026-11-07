/* SPDX-License-Identifier: MIT
 * Field specification: Imagination MIT rogue/ppp.xml, 2022.
 * Complete state observations: stock m1.11 capture06, REA
 * ev_98025efd600eeadb70a4a03436a076c77d8ff6024deb0817ca55ab917ab7fdcf.
 * No captured VA, executable blob, or unknown resource is substituted.
 */
#include "native_triangle_ppp.h"
static void put(uint8_t *p,unsigned n,uint32_t v)
{ for(unsigned j=0;j<4;j++)p[4*n+j]=(uint8_t)(v>>(8*j)); }
static int address(uint32_t p)
{ return p<0x40000000u && !(p&15u); }
int a04_m111_build_triangle_prefix(void *out,size_t bytes)
{
    uint8_t made[A04_M111_TRIANGLE_PREFIX_BYTES]={0};
    if(!out || bytes!=sizeof(made))return 1;
    /* ISP/FA/FB, region clip, output selection, varying0..2, control,
     * stream-out size present. No fragment program is present. */
    put(made,0,7u|(1u<<10)|(0x3fu<<17));
    put(made,1,(1u<<21)|(1u<<18)); /* tag write disabled, ISP B present */
    put(made,2,(1u<<28)|(1u<<21)|(7u<<17)); /* LINE, no depth writes, ALWAYS */
    put(made,3,7u<<25); /* stencil ALWAYS/KEEP */
    put(made,4,1u<<31); /* outside region: zero extents */
    put(made,6,(4u<<24)|(1u<<16)); /* four outputs, RHW */
    put(made,10,1u<<6); /* pretransformed */
    for(unsigned i=0;i<sizeof(made);i++)((uint8_t *)out)[i]=made[i];
    return 0;
}
int a04_m111_build_triangle_ppp(void *out,size_t bytes,
    const struct a04_m111_triangle_ppp_input *p)
{
    uint8_t made[A04_M111_TRIANGLE_TEXTURE_PPP_BYTES]={0};unsigned n=0;
    if(!out || !p || p->textured>1 ||
       bytes!=(p->textured?A04_M111_TRIANGLE_TEXTURE_PPP_BYTES:A04_M111_TRIANGLE_SOLID_PPP_BYTES) ||
       !address(p->fragment_pds) || !address(p->texuni_code) ||
       !address(p->varying_pds) || !address(p->uniform_pds) ||
       (p->textured?!address(p->texture_pds):p->texture_pds!=0))return 1;
    /* Full fixed state including viewport, clamp, three fragment PDS pointers
     * and optional texture data pointer. No field is copied from an input blob. */
    put(made,n++,7u|(7u<<5)|(1u<<9)|(3u<<10)|(1u<<16)|(0x3fu<<17)|
        (p->textured?(1u<<8):0));
    put(made,n++,(15u<<22)|(1u<<18)|(1u<<16)); /* upass15, B, scissor */
    put(made,n++,(1u<<21)|(7u<<17)|(15u<<8)); /* triangle, depth ALWAYS */
    put(made,n++,7u<<25);
    put(made,n++,p->scissor_index);
    put(made,n++,p->fragment_pds);put(made,n++,p->texuni_code);
    put(made,n++,(1u<<23)|(p->textured?(2u<<16):0)|1u);
    put(made,n++,(1u<<23)|(1u<<14)); /* shared16, merge disabled */
    put(made,n++,p->varying_pds);
    if(p->textured)put(made,n++,p->texture_pds);
    put(made,n++,p->uniform_pds);
    /* Observed fixed board profile uses native 16px region clips here.
     * Do not generalize dimensions using another IP generation's tile units. */
    put(made,n++,(1u<<31)|49u);put(made,n++,37u);
    put(made,n++,0x43c80000u);put(made,n++,0x43c80000u); /* x 400/400 */
    put(made,n++,0x43960000u);put(made,n++,0x43960000u); /* y 300/300 */
    put(made,n++,0x3f000000u);put(made,n++,0x3f000000u); /* z .5/.5 */
    put(made,n++,0x33d6bf95u); /* observed W clamp, preserve exact float */
    put(made,n++,(4u<<24)|(1u<<16));
    put(made,n++,0);put(made,n++,0);put(made,n++,0); /* no varyings */
    put(made,n++,(1u<<10)|(1u<<9)|(3u<<7)|(1u<<5)); /* front+rear, vtx2, W clamp */
    put(made,n++,0); /* no stream-out */
    if(n*4u!=bytes)return 1;
    for(unsigned i=0;i<bytes;i++)((uint8_t *)out)[i]=made[i];
    return 0;
}
