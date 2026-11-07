/* SPDX-License-Identifier: MIT
 * Services field stores from board libsrv_um.so SHA256
 * d32a4a224d06302d9bec0a66cbb7d17dec663d3985f1c8592f5bb77353ca1d4b:
 * RGXKickTA 0x126354, FUN_0012877c. These routines do not construct the
 * unobserved register prefix/tail and cannot submit a task.
 */
#include "native_record_fields.h"
static int valid(const void *p,size_t n,const void *f,size_t m)
{
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)f;
    return p && f && (a<=b ? b-a>=n : a-b>=m);
}
static void word(void *p,size_t at,uint64_t v,unsigned bytes)
{ unsigned i;for(i=0;i<bytes;i++)((uint8_t*)p)[at+i]=(uint8_t)(v>>(i*8u)); }
uint32_t a04_m111_ta_flags(uint32_t f,uint32_t rt)
{
    uint32_t v=(rt&2u)?0u:1u;
    if(f&9u)v|=2u;
    if(f&(1u<<3))v|=0x100u;
    if(f&(1u<<6))v|=0x100000u;
    if(f&(1u<<11))v|=4u;
    if(f&(1u<<13))v|=0x400000u;
    if(f&(1u<<20))v|=0x800000u;
    return v;
}
uint32_t a04_m111_3d_flags(uint32_t f,uint32_t rt,int context)
{
    uint32_t v=context?0x8000u:0u;
    if(f&(1u<<9))v|=0x80u;
    if(f&(1u<<10))v|=0x100u;
    if(f&(1u<<7))v|=0x20u;
    if(f&(1u<<15))v|=0x80000u;
    if(f&(1u<<23))v|=0x4000000u;
    if(f&(1u<<11))v|=1u;
    if(f&(1u<<3))v|=(rt&2u)?2u:0x4002u;
    if(f&(1u<<14))v|=0x40000u;
    return v;
}
int a04_m111_patch_ta_fields(void *p,size_t bytes,const struct a04_m111_ta_fields *f)
{
    if(bytes!=A04_M111_TA_BYTES || !valid(p,bytes,f,sizeof(*f)))return 1;
    word(p,0x08,f->word08,8);word(p,0x30,f->word30,8);
    word(p,0x48,f->flags,4);word(p,0x50,f->frame,4);word(p,0x58,f->hwrt,4);
    word(p,0x5c,f->zbuffer,4);word(p,0x60,f->sbuffer,4);word(p,0x64,f->msaa,4);
    word(p,0x68,f->pr_fence_addr,4);word(p,0x6c,f->pr_fence_value,4);
    word(p,0x70,f->overlap_addr,4);word(p,0x74,f->overlap_value,4);word(p,0x78,f->dummy_region,8);
    return 0;
}
int a04_m111_patch_3d_fields(void *p,size_t bytes,const struct a04_m111_3d_fields *f)
{
    if(bytes!=A04_M111_3D_BYTES || !valid(p,bytes,f,sizeof(*f)))return 1;
    word(p,0,f->word00,4);word(p,0x1c,f->word1c,4);word(p,0x20,f->word20,4);
    word(p,0x128,f->dummy_stencil,8);word(p,0x130,f->flags,4);word(p,0x134,f->frame,4);
    word(p,0x138,f->hwrt,4);word(p,0x13c,f->zbuffer,4);word(p,0x140,f->sbuffer,4);word(p,0x144,f->msaa,4);
    word(p,0x148,f->zls_stride,4);word(p,0x14c,f->sls_stride,4);
    return 0;
}
int a04_m111_patch_ta_registers(void *p,size_t bytes,const struct a04_m111_ta_registers *r)
{
    if(bytes!=A04_M111_TA_BYTES || !valid(p,bytes,r,sizeof(*r)))return 1;
    word(p,0,r->ppp_ctrl,8);word(p,0x10,r->vdm_stream,8);
    word(p,0x18,r->border_table,8);word(p,0x20,r->word20,8);word(p,0x28,r->tpu,8);
    word(p,0x38,r->word38,4);word(p,0x40,r->pds_code40,4);
    return 0;
}
int a04_m111_patch_3d_pbe(void *p,size_t bytes,const uint64_t csr[8][2])
{
    unsigned i,j;
    if(bytes!=A04_M111_3D_BYTES || !valid(p,bytes,csr,8u*2u*8u))return 1;
    for(i=0;i<8;i++)for(j=0;j<2;j++)word(p,0x68u+i*16u+j*8u,csr[i][j],8);
    return 0;
}
int a04_m111_patch_3d_background(void *p,size_t bytes,const uint64_t a[3],const uint64_t b[3])
{
    unsigned i;
    if(bytes!=A04_M111_3D_BYTES || !valid(p,bytes,a,24u) || !valid(p,bytes,b,24u))return 1;
    for(i=0;i<3;i++){word(p,0xf8u+i*8u,a[i],8);word(p,0x110u+i*8u,b[i],8);}
    return 0;
}
