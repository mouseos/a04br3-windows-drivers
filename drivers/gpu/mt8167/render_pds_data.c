/* SPDX-License-Identifier: MIT
 * Constant-map placement and DOUTU encoding checked against Mesa PowerVR
 * ef11349e8e pvr_arch_cmd_buffer.c, pvr_pds.h and pvr_rogue_pds_defs.h. */
#include "render_pds_data.h"
#define LIMIT (UINT64_C(1)<<40)
#define DOUTU_CONTROL_MASK UINT64_C(0x000003fe00000000)
static void put32(uint8_t *p,uint32_t x)
{ p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);p[2]=(uint8_t)(x>>16);p[3]=(uint8_t)(x>>24); }
static void put64(uint8_t *p,uint64_t x) { put32(p,(uint32_t)x);put32(p+4,(uint32_t)(x>>32)); }
static int owned(const struct a04_render_owned_range *r,size_t n,uint64_t va,uint64_t len,unsigned access)
{
    size_t i;
    if (!va || va>=LIMIT || !len || len>LIMIT-va) return 0;
    for(i=0;i<n;++i)
        if ((r[i].access&access)==access && va>=r[i].gpu_va && len<=r[i].size &&
            va-r[i].gpu_va<=r[i].size-len) return 1;
    return 0;
}
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
int a04_render_make_pds_data(const struct a04_render_pds_input *p,
    const struct a04_render_owned_range *r,size_t n,struct a04_render_pds_data *out)
{
    struct a04_render_pds_data result={{0},{0},{0},{0}};
    uint64_t a[5],length[5]={4,12,12,8192,49152};size_t i,j;
    if (!p || !out || a04_render_validate_owned_ranges(r,n) ||
        (p->doutu_control&~DOUTU_CONTROL_MASK) || (p->shader_gpu&3u) ||
        (p->spill_info_gpu&7u) || (p->scratch_info_gpu&7u)) return -1;
    /* DOUTU takes a 32-bit USC heap-relative execution offset. */
    if ((p->shader_gpu >> 32)!=UINT64_C(0xe0) ||
        !owned(r,n,p->shader_gpu,4,A04_RENDER_RANGE_READ|A04_RENDER_RANGE_EXEC) ||
        !owned(r,n,p->spill_info_gpu,12,A04_RENDER_RANGE_READ) ||
        !owned(r,n,p->scratch_info_gpu,12,A04_RENDER_RANGE_READ) ||
        !owned(r,n,p->spill_buffer_gpu,8192,A04_RENDER_RANGE_READ|A04_RENDER_RANGE_WRITE) ||
        !owned(r,n,p->scratch_buffer_gpu,49152,A04_RENDER_RANGE_READ|A04_RENDER_RANGE_WRITE)) return -1;
    a[0]=p->shader_gpu;a[1]=p->spill_info_gpu;a[2]=p->scratch_info_gpu;
    a[3]=p->spill_buffer_gpu;a[4]=p->scratch_buffer_gpu;
    for(i=0;i<5;++i) for(j=0;j<i;++j) if(overlap(a[i],length[i],a[j],length[j])) return -1;
    put64(result.vertex,p->doutu_control|((uint32_t)p->shader_gpu&UINT32_C(0xfffffffc)));
    put32(result.vertex+8,p->vertex_literal);
    put64(result.descriptor,p->spill_info_gpu);put64(result.descriptor+8,p->scratch_info_gpu);
    put32(result.descriptor+16,p->descriptor_literal0);put32(result.descriptor+20,p->descriptor_literal1);
    put64(result.spill_info,p->spill_buffer_gpu);put32(result.spill_info+8,4);
    put64(result.scratch_info,p->scratch_buffer_gpu);put32(result.scratch_info+8,24);
    *out=result;return 0;
}
int a04_render_make_fragment_pds(uint64_t shader_gpu,uint64_t control,
    const struct a04_render_owned_range *r,size_t n,uint8_t out[48])
{
    uint8_t result[48]={0};size_t i;
    if (!out || a04_render_validate_owned_ranges(r,n) ||
        (control&~DOUTU_CONTROL_MASK) || (control&(UINT64_C(3)<<33)) ||
        (shader_gpu&3u) || (shader_gpu>>32)!=UINT64_C(0xe0) ||
        !owned(r,n,shader_gpu,4,A04_RENDER_RANGE_READ|A04_RENDER_RANGE_EXEC)) return -1;
    put64(result,control|((uint32_t)shader_gpu&UINT32_C(0xfffffffc)));
    put32(result+32,UINT32_C(0xf4000002));
    for(i=0;i<sizeof(result);++i) out[i]=result[i];
    return 0;
}
