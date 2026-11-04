/* SPDX-License-Identifier: MIT
 * Fixed GE8300 LD/ST constant encoding from Mesa ef11349e8e,
 * pvr_pds_generate_single_ldst_instruction and pvr_rogue_pds_defs.h. */
#include "render_context_pds.h"
static void put64(uint8_t *p,uint64_t v)
{ unsigned i;for(i=0;i<8;++i) p[i]=(uint8_t)(v>>(8u*i)); }
static void put32(uint8_t *p,uint32_t v)
{ unsigned i;for(i=0;i<4;++i) p[i]=(uint8_t)(v>>(8u*i)); }
int a04_render_make_context_pt_data(uint64_t gpu,const struct a04_render_owned_range *ranges,
    size_t count,struct a04_render_context_pt_data *out)
{
    struct a04_render_context_pt_data data={{0},{0}};size_t i;int owned=0;
    static const unsigned store_off[3]={0,60,120},store_count[3]={15,15,2};
    static const unsigned load_off[3]={0,48,96},load_count[3]={6,6,4};
    if (!out || (gpu&15u) || !gpu || gpu>=(UINT64_C(1)<<40) ||
        128u>(UINT64_C(1)<<40)-gpu || a04_render_validate_owned_ranges(ranges,count)) return -1;
    for (i=0;i<count;++i)
        if ((ranges[i].access&(A04_RENDER_RANGE_READ|A04_RENDER_RANGE_WRITE))==(A04_RENDER_RANGE_READ|A04_RENDER_RANGE_WRITE) &&
            gpu>=ranges[i].gpu_va && ranges[i].size>=128u && gpu-ranges[i].gpu_va<=ranges[i].size-128u) ++owned;
    if (owned!=1) return -1;
    for (i=0;i<3;++i) {
        uint64_t st=(gpu+store_off[i])|((uint64_t)store_count[i]<<40)|((uint64_t)(32u+15u*i)<<46);
        uint64_t ld=(gpu+load_off[i])|((uint64_t)load_count[i]<<41)|((uint64_t)(16u+6u*i)<<47);
        put64(data.store+i*8u,st);put64(data.resume+i*8u,ld);
    }
    *out=data;return 0;
}
int a04_render_make_context_sr_data(uint64_t state,uint64_t shader,uint64_t control,
    const struct a04_render_owned_range *ranges,size_t count,uint8_t out[64])
{
    uint8_t data[64]={0};size_t i;int st=0,code=0;
    if (!out || !state || (state&15u) || state>=(UINT64_C(1)<<40) || 16432u>(UINT64_C(1)<<40)-state ||
        (shader>>32)!=UINT64_C(0xe0) || (shader&3u) || (control&~UINT64_C(0x3fe00000000)) ||
        (control&(UINT64_C(3)<<33)) || a04_render_validate_owned_ranges(ranges,count)) return -1;
    for (i=0;i<count;++i) {
        const struct a04_render_owned_range *r=&ranges[i];
        if ((r->access&3u)==3u && state>=r->gpu_va && r->size>=16432u && state-r->gpu_va<=r->size-16432u) ++st;
        if ((r->access&5u)==5u && shader>=r->gpu_va && r->size>=4u && shader-r->gpu_va<=r->size-4u) ++code;
    }
    if (st!=1 || code!=1) return -1;
    put32(data,(uint32_t)(state>>32));put32(data+4,(uint32_t)state);put32(data+8,2);
    put32(data+16,(uint32_t)((state+48u)>>32));put32(data+20,(uint32_t)(state+48u));put32(data+24,UINT32_C(0x80004002));
    put64(data+32,((uint32_t)shader&UINT32_C(0xfffffffc))|control);
    for(i=0;i<sizeof(data);++i) out[i]=data[i];
    return 0;
}
int a04_render_make_context_pds_code(struct a04_render_context_pds_code *out)
{
    struct a04_render_context_pds_code c={{0},{0},{0}};size_t i;
    static const uint32_t st[]={0xd0800000u,0xd0800001u,0xd0800002u,0xd1000000u,0xd3000000u};
    static const uint32_t ld[]={0xd3800010u,0xd0000000u,0xd0000001u,0xd0000002u,0xd1000000u,0xd3000000u};
    static const uint32_t sr[]={0xf0020001u,0xf0060201u,0xf4000402u};
    if (!out) return -1;
    for(i=0;i<sizeof(st)/sizeof(st[0]);++i) put32(c.pt_store+i*4u,st[i]);
    for(i=0;i<sizeof(ld)/sizeof(ld[0]);++i) put32(c.pt_resume+i*4u,ld[i]);
    for(i=0;i<sizeof(sr)/sizeof(sr[0]);++i) put32(c.sr+i*4u,sr[i]);
    *out=c;return 0;
}
