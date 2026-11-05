/* SPDX-License-Identifier: MIT
 * Encodings from Mesa ef11349e8e pvr_arch_job_context.c and rogue CSB XML. */
#include "render_context_static.h"
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
static int owns(const struct a04_render_owned_range *r,size_t count,uint64_t a,uint64_t n,int writable)
{
    size_t i;unsigned matches=0;
    for(i=0;i<count;++i) if ((r[i].access&A04_RENDER_RANGE_READ) &&
        !!(r[i].access&A04_RENDER_RANGE_WRITE)==!!writable && a>=r[i].gpu_va &&
        n<=r[i].size && a-r[i].gpu_va<=r[i].size-n) ++matches;
    return matches==1;
}
int a04_render_make_context_static(const struct a04_render_context_static_input *p,
    const struct a04_render_owned_range *r,size_t count,struct rogue_fwif_geom_registers_caswitch *out)
{
    struct rogue_fwif_geom_registers_caswitch result={0};uint64_t a[18],n[18];size_t i,j,k=0;
    if (!p || !out || a04_render_validate_owned_ranges(r,count) || (p->vdm_state&15u) || (p->geom_state&15u) ||
        !owns(r,count,p->vdm_state,92,1) || !owns(r,count,p->geom_state,484,1)) return -1;
    a[k]=p->vdm_state;n[k++]=92;a[k]=p->geom_state;n[k++]=484;
    for(i=0;i<2;++i) {
        const struct a04_render_context_static_slot *s=&p->slot[i];
        uint64_t x[8]={s->pt_store_data,s->pt_store_code,s->pt_resume_data,s->pt_resume_code,
            s->sr_store_data,s->sr_store_code,s->sr_resume_data,s->sr_resume_code};
        uint64_t z[8]={32,32,32,32,64,16,64,16};
        if (s->pt_store_code!=s->pt_store_data+32u || s->pt_resume_code!=s->pt_resume_data+32u) return -1;
        for(j=0;j<8;++j) {
            if ((x[j]>>32)!=UINT64_C(0xda) || (x[j]&15u) || !owns(r,count,x[j],z[j],0)) return -1;
            a[k]=x[j];n[k++]=z[j];
        }
        result.geom_state[i].geom_reg_vdm_context_store_task0=((uint64_t)((uint32_t)s->sr_store_data|5u)<<32)|UINT32_C(0x22001004);
        result.geom_state[i].geom_reg_vdm_context_store_task1=(uint32_t)s->sr_store_code;
        result.geom_state[i].geom_reg_vdm_context_store_task2=((uint64_t)(uint32_t)s->pt_store_data<<32)|UINT32_C(0x420);
        result.geom_state[i].geom_reg_vdm_context_resume_task0=((uint64_t)((uint32_t)s->sr_resume_data|5u)<<32)|UINT32_C(0x20001004);
        result.geom_state[i].geom_reg_vdm_context_resume_task1=(uint32_t)s->sr_resume_code;
        result.geom_state[i].geom_reg_vdm_context_resume_task2=((uint64_t)(uint32_t)s->pt_resume_data<<32)|UINT32_C(0x420);
    }
    for(i=0;i<k;++i) for(j=0;j<i;++j) if(overlap(a[i],n[i],a[j],n[j])) return -1;
    result.geom_reg_vdm_context_state_base_addr=p->vdm_state;
    result.geom_reg_ta_context_state_base_addr=p->geom_state;
    *out=result;return 0;
}
