/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_record_fields.h"
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(void)
{
    uint8_t ta[A04_M111_TA_BYTES+2],frag[A04_M111_3D_BYTES+2];unsigned i;
    struct a04_m111_ta_fields t={0};struct a04_m111_3d_fields f={0};
    struct a04_m111_ta_registers regs={0x202,UINT64_C(0x6500100000),UINT64_C(0x1940100000),0,0x20,0x5f00,0x4000};
    uint64_t csr[8][2]={{0}},bg_f8[3]={1,2,3},bg_110[3]={4,5,6};
    t.word08=UINT64_C(0x1122334455667788);t.word30=UINT64_C(0x8877665544332211);
    t.flags=a04_m111_ta_flags(9,0);t.frame=17;t.hwrt=0xc0090100;t.pr_fence_addr=0xc0080300;t.pr_fence_value=5;
    t.dummy_region=UINT64_C(0x6500600000);
    memset(ta,0xa5,sizeof(ta));
    assert(!a04_m111_patch_ta_fields(ta+1,A04_M111_TA_BYTES,&t));
    assert(ta[0]==0xa5 && ta[sizeof(ta)-1]==0xa5);
    assert(u32(ta+1+8)==0x55667788 && u32(ta+1+12)==0x11223344);
    assert(u32(ta+1+0x48)==0x103 && u32(ta+1+0x50)==17 && u32(ta+1+0x58)==t.hwrt);
    assert(u32(ta+1+0x78)==0x00600000 && u32(ta+1+0x7c)==0x65);
    /* Unknown intervals must survive, including presumed alignment holes. */
    for(i=0;i<A04_M111_TA_BYTES;i++)
        if(i<8 || (i>=0x10 && i<0x30) || (i>=0x38 && i<0x48) ||
           (i>=0x4c && i<0x50) || (i>=0x54 && i<0x58) || i>=0x80)assert(ta[i+1]==0xa5);
    assert(!a04_m111_patch_ta_registers(ta+1,A04_M111_TA_BYTES,&regs));
    assert(u32(ta+1)==0x202 && u32(ta+1+0x10)==0x100000 && u32(ta+1+0x14)==0x65);
    assert(u32(ta+1+0x28)==0x20 && u32(ta+1+0x38)==0x5f00 && u32(ta+1+0x40)==0x4000);
    assert(u32(ta+1+0x3c)==0xa5a5a5a5 && u32(ta+1+0x44)==0xa5a5a5a5 && ta[1+0x80]==0xa5);
    memset(frag,0x5a,sizeof(frag));f.word00=0x1ffc;f.word20=0x90000;
    f.dummy_stencil=UINT64_C(0x6500300001);f.hwrt=t.hwrt;f.frame=17;
    f.flags=a04_m111_3d_flags(8,0,1);
    f.zls_stride=0x123400;f.sls_stride=0x234500;
    assert(!a04_m111_patch_3d_fields(frag+1,A04_M111_3D_BYTES,&f));
    assert(u32(frag+1)==0x1ffc && u32(frag+1+0x130)==0xc002 && u32(frag+1+0x138)==t.hwrt);
    for(i=0;i<A04_M111_3D_BYTES;i++)
        if((i>=4 && i<0x1c) || (i>=0x24 && i<0x128))assert(frag[i+1]==0x5a);
    assert(u32(frag+1+0x148)==f.zls_stride && u32(frag+1+0x14c)==f.sls_stride);
    assert(frag[0]==0x5a && frag[sizeof(frag)-1]==0x5a);
    for(i=0;i<8;i++){csr[i][0]=UINT64_C(0x1122334400000000)+i;csr[i][1]=UINT64_C(0x5566778800000010)+i;}
    assert(!a04_m111_patch_3d_pbe(frag+1,A04_M111_3D_BYTES,csr));
    assert(u32(frag+1+0x68)==0 && u32(frag+1+0x6c)==0x11223344);
    assert(u32(frag+1+0xd8)==7 && u32(frag+1+0xe0)==0x17);
    assert(frag[1+0x67]==0x5a && frag[1+0xe8]==0x5a);
    assert(!a04_m111_patch_3d_background(frag+1,A04_M111_3D_BYTES,bg_f8,bg_110));
    assert(u32(frag+1+0xf8)==1 && u32(frag+1+0x108)==3 && u32(frag+1+0x120)==6);
    assert(u32(frag+1+0x128)==0x00300001 && u32(frag+1+0x12c)==0x65);
    assert(a04_m111_patch_ta_fields(ta+1,A04_M111_TA_BYTES-1,&t));
    assert(a04_m111_patch_3d_fields(frag+1,A04_M111_3D_BYTES,(void*)(frag+1)));
    assert(a04_m111_ta_flags(0,2)==0);
    assert(a04_m111_ta_flags(1u<<11,2)==4 && a04_m111_ta_flags(1u<<20,2)==0x800000);
    assert(a04_m111_3d_flags(8,2,0)==2 && a04_m111_3d_flags(1u<<23,0,0)==0x4000000);
    assert(a04_m111_3d_flags(1u<<9,0,0)==0x80 && a04_m111_3d_flags(1u<<11,0,0)==1);
    return 0;
}
