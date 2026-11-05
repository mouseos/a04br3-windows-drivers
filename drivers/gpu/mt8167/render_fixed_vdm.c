/* SPDX-License-Identifier: MIT
 * Packet fields derived from Mesa csbgen/rogue/vdm.xml, ef11349e8e. */
#include "render_fixed_vdm.h"
static void put32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
int a04_render_make_fixed_vdm(const struct a04_render_fixed_vdm_input *p,
    const struct a04_render_owned_range *ranges,size_t count,uint8_t out[60])
{
    uint64_t a[6],n[6]={16,16,16,16,100,12};uint32_t words[15];uint8_t data[60];size_t i,j;
    if (!p || !out || a04_render_validate_owned_ranges(ranges,count)) return -1;
    a[0]=p->descriptor_data;a[1]=p->descriptor_code;a[2]=p->vertex_data;
    a[3]=p->vertex_code;a[4]=p->ppp_state;a[5]=p->ppp_finish;
    for (i=0;i<6;++i) {
        unsigned matches=0;
        if (!a[i] || a[i]>=(UINT64_C(1)<<40) || n[i]>(UINT64_C(1)<<40)-a[i] ||
            (a[i]&(i<4?15u:3u)) || (i<4 && (a[i]>>32)!=UINT64_C(0xda))) return -1;
        for(j=0;j<i;++j) if(overlap(a[i],n[i],a[j],n[j])) return -1;
        for(j=0;j<count;++j) if ((ranges[j].access&A04_RENDER_RANGE_READ) && !(ranges[j].access&A04_RENDER_RANGE_WRITE) &&
            a[i]>=ranges[j].gpu_va && n[i]<=ranges[j].size && a[i]-ranges[j].gpu_va<=ranges[j].size-n[i]) ++matches;
        if(matches!=1) return -1;
    }
    words[0]=UINT32_C(0x20010001);words[1]=(uint32_t)a[0];words[2]=(uint32_t)a[1];
    words[3]=(25u<<8)|(uint32_t)(a[4]>>32);words[4]=(uint32_t)a[4];
    words[5]=UINT32_C(0x4c000f80);words[6]=(uint32_t)a[2];words[7]=(uint32_t)a[3];
    words[8]=5;words[9]=UINT32_C(0x20000441);words[10]=UINT32_C(0x68000600);words[11]=3;
    words[12]=(3u<<8)|(uint32_t)(a[5]>>32);words[13]=(uint32_t)a[5];words[14]=UINT32_C(0xc0000000);
    for(i=0;i<15;++i) put32(data+4*i,words[i]);
    for(i=0;i<60;++i) out[i]=data[i];
    return 0;
}
