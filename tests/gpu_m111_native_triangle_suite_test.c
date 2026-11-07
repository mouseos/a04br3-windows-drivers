/* SPDX-License-Identifier: MIT
 * CPU dependency graph/relocation proof, not GPU execution.
 * Every active VDM/PPP/PDS DMA/task/image reference is followed into an owned
 * span. All three heaps and target are then moved and the proof repeated. */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "native_triangle_suite.h"
#include "native_triangle_ppp.h"
#include "native_triangle_pds.h"
static unsigned u32(const uint8_t *p)
{ return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24; }
static uint64_t u64(const uint8_t *p)
{ return u32(p)|(uint64_t)u32(p+4)<<32; }
static unsigned offset(uint64_t v,uint64_t base,unsigned bytes,unsigned limit)
{ assert(v>=base && v-base<=limit && bytes<=limit-(v-base));return (unsigned)(v-base); }
static void closure(const struct a04_m111_triangle_suite *j,const struct a04_m111_clear_job_input *in)
{
    unsigned frame,i;uint64_t pb=in->pds_page_gpu-UINT64_C(0xa800000000);
    for(frame=0;frame<2;frame++){
        const uint8_t *ta=j->record[frame].ta,*three=j->record[frame].three;
        const uint8_t *vdm=j->data+offset(u64(ta+16),in->data_page_gpu,84,sizeof(j->data));
        unsigned np=0;
        for(i=0;i<3;i++){
            unsigned at=i==0?0:i==1?14:18,n=(u32(vdm+at*4)>>8)&255;
            uint64_t a=((uint64_t)(u32(vdm+at*4)&255)<<32)|u32(vdm+at*4+4);
            const uint8_t *pp=j->data+offset(a,in->data_page_gpu,n*4,sizeof(j->data));
            if(i==0){assert(n==12 && !(u32(pp)&(15u<<6)));continue;}
            if(i==2){assert(n==3 && u32(pp)==1u<<25);continue;}
            assert(n==(frame?27u:26u));
            /* Fragment data32 + code4; source tasks USC0/5 temps. */
            const uint8_t *ps=j->pds+offset(UINT64_C(0xa800000000)+u32(pp+20),in->pds_page_gpu,36,sizeof(j->pds));
            unsigned task=(unsigned)(u64(ps)&0x3fffffffu);
            offset(UINT64_C(0xba00000000)+task,in->usc_page_gpu,frame?64:16,sizeof(j->usc));
            assert((u64(ps)>>35)==(frame?2u:0u));
            assert(u32(ps+32)==0xf4000002u);
            /* Same one-DMA program used for uniform and optional texture data. */
            assert(u32(j->pds+offset(UINT64_C(0xa800000000)+u32(pp+24),in->pds_page_gpu,4,sizeof(j->pds)))==0xf4020000u);
            for(unsigned t=0;t<(frame?2u:1u);t++){
                const uint8_t *pd=j->pds+offset(UINT64_C(0xa800000000)+u32(pp+(10+t)*4),
                    in->pds_page_gpu,16,sizeof(j->pds));
                unsigned ctrl=u32(pd+8),words=ctrl&4095;
                assert(ctrl&(1u<<28));assert(ctrl&(1u<<31));
                offset(u64(pd),in->data_page_gpu,words*4,sizeof(j->data));np++;
                if(frame && t==0){
                    const uint8_t *img=j->data+offset(u64(pd)+16,in->data_page_gpu,16,sizeof(j->data));
                    uint64_t texel=((u64(img+8)>>16)&UINT64_C(0x3fffffffff))<<2;
                    unsigned off=offset(texel,in->data_page_gpu,4,sizeof(j->data));
                    assert(u32(j->data+off)==0xffcc9933u);
                }
            }
        }
        assert(np==(frame?2u:1u));
        for(i=0;i<2;i++){
            unsigned at=i?9:2;const uint8_t *pd=j->pds+offset(UINT64_C(0xa800000000)+u32(vdm+(at+1)*4),in->pds_page_gpu,32,sizeof(j->pds));
            offset(UINT64_C(0xba00000000)+(u64(pd)&0x3fffffffu),in->usc_page_gpu,i?40:32,sizeof(j->usc));
            unsigned bytes=i?24:24;
            offset(u64(pd+16),in->data_page_gpu,bytes,sizeof(j->data));
            offset(UINT64_C(0xa800000000)+u32(vdm+(at+2)*4),in->pds_page_gpu,12,sizeof(j->pds));
        }
        offset(u64(three+0x38),in->data_page_gpu,16,sizeof(j->data));
        offset(u64(three+0x40),in->data_page_gpu,12,sizeof(j->data));
        assert(u32(ta+0x48)==0x100003u && u32(ta+0x6c)==frame);
        assert(u32(three+4)==0xff000000u);
        /* EOT PBE destination follows the owner's mapped output VA. */
        assert((u64(j->pds+0x88)&UINT64_C(0x3fffffffff))==in->output_gpu/4);
    }
    assert(pb==u32(j->record[0].ta+0x40)-0xc0u);
}
int main(void)
{
    struct a04_m111_triangle_suite j,before;_Alignas(8) uint8_t scratch[sizeof(j)];
    struct a04_m111_clear_job_input in={UINT64_C(0x8000000000),UINT64_C(0x65c0358000),
        UINT64_C(0xba00010000),UINT64_C(0xa800010000),UINT64_C(0x65c0340000),
        UINT64_C(0x65c0340080),0xc00848c0,0xc0084c00,{17,34,51,255}};
    assert(!a04_m111_build_triangle_suite(&j,&in,scratch,sizeof(scratch)));closure(&j,&in);
    in.data_page_gpu+=0x2000;in.usc_page_gpu+=0x4000;in.pds_page_gpu+=0x8000;in.output_gpu+=0x400000;
    assert(!a04_m111_build_triangle_suite(&j,&in,scratch,sizeof(scratch)));closure(&j,&in);before=j;
    assert(a04_m111_build_triangle_suite(&j,&in,&j,sizeof(j)));
    assert(a04_m111_build_triangle_suite(&j,&in,scratch,sizeof(scratch)-1));
    in.data_page_gpu=in.dummy_region_gpu;
    assert(a04_m111_build_triangle_suite(&j,&in,scratch,sizeof(scratch)));
    assert(!memcmp(&j,&before,sizeof(j)));
    unsigned solid=0,texture=0;
    for(unsigned p=0;p<480000;p++){
        uint32_t a=a04_m111_triangle_expected_pixel(0,p),b=a04_m111_triangle_expected_pixel(1,p);
        assert(a==0xff6633ccu || a==0xff000000u);assert(b==0xffcc9933u || b==0xff000000u);
        solid+=a==0xff6633ccu;texture+=b==0xffcc9933u;
    }
    assert(solid==240000 && texture==240000);
    assert(a04_m111_triangle_expected_pixel(1,480000)==0xa5a5a5a5u);
    puts("triangle suite CPU dependency closure, independent heap relocation, unchanged-error and half-plane pixel oracle: PASS; no GPU");
}
