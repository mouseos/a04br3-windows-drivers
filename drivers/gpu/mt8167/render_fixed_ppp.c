/* SPDX-License-Identifier: MIT
 * Fixed fields follow Mesa csbgen/rogue/ppp.xml and pvr_arch_cmd_buffer.c,
 * ef11349e8e. W clamp is IEEE-754 binary32 1.0e-13f. */
#include "render_fixed_ppp.h"
static void put32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
int a04_render_make_fixed_ppp(const struct a04_render_fixed_ppp_input *p,
    const struct a04_render_owned_range *ranges,size_t count,struct a04_render_fixed_ppp *out)
{
    const uint32_t fixed[25]={0x007f0e67,0x00050000,0x022e0f00,0x0e000000,0,0,0,0x01000000,0x00804000,0,
        0x80000003,3,0x42000000,0x42000000,0x42000000,0x42000000,0,0x3f800000,0x29e12e13,0x05040000,0,0,0,0x6a0,0};
    uint64_t a[3],n[3]={48,16,32};struct a04_render_fixed_ppp result;size_t i,j;
    if (!p || !out || a04_render_validate_owned_ranges(ranges,count)) return -1;
    a[0]=p->fragment_pds;a[1]=p->uniform_code;a[2]=p->uniform_data;
    for(i=0;i<3;++i) {
        unsigned matches=0;
        if (!a[i] || a[i]>=(UINT64_C(1)<<40) || n[i]>(UINT64_C(1)<<40)-a[i] || (a[i]&15u) || (a[i]>>32)!=UINT64_C(0xda)) return -1;
        for(j=0;j<i;++j) if(overlap(a[i],n[i],a[j],n[j])) return -1;
        for(j=0;j<count;++j) if ((ranges[j].access&A04_RENDER_RANGE_READ) && !(ranges[j].access&A04_RENDER_RANGE_WRITE) &&
            a[i]>=ranges[j].gpu_va && n[i]<=ranges[j].size && a[i]-ranges[j].gpu_va<=ranges[j].size-n[i]) ++matches;
        if(matches!=1) return -1;
    }
    for(i=0;i<25;++i) put32(result.state+4*i,fixed[i]);
    put32(result.state+20,(uint32_t)a[0]);put32(result.state+24,(uint32_t)a[1]);put32(result.state+36,(uint32_t)a[2]);
    put32(result.finish,UINT32_C(0x02000000));put32(result.finish+4,UINT32_C(0x00040001));put32(result.finish+8,0);
    *out=result;return 0;
}
