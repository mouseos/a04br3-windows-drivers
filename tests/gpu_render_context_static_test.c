/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_context_static.h"
int main(void)
{
    struct a04_render_owned_range ranges[]={
        {UINT64_C(0x8000000000),0x100000,4096,3},
        {UINT64_C(0xda00001000),0x101000,4096,1}};
    struct a04_render_context_static_input p={0};
    struct rogue_fwif_geom_registers_caswitch out,old;unsigned i;
    p.vdm_state=UINT64_C(0x8000000000);p.geom_state=UINT64_C(0x8000000100);
    for(i=0;i<2;i++) {
        uint64_t x=UINT64_C(0xda00001000)+(uint64_t)i*0x200;
        struct a04_render_context_static_slot *s=&p.slot[i];
        s->pt_store_data=x;s->pt_store_code=x+32;s->pt_resume_data=x+64;s->pt_resume_code=x+96;
        s->sr_store_data=x+128;s->sr_store_code=x+192;s->sr_resume_data=x+208;s->sr_resume_code=x+272;
    }
    assert(!a04_render_make_context_static(&p,ranges,2,&out));
    assert(out.geom_reg_vdm_context_state_base_addr==p.vdm_state && out.geom_reg_ta_context_state_base_addr==p.geom_state);
    assert(out.geom_state[0].geom_reg_vdm_context_store_task0==UINT64_C(0x0000108522001004));
    assert(out.geom_state[0].geom_reg_vdm_context_store_task1==0x10c0);
    assert(out.geom_state[0].geom_reg_vdm_context_store_task2==UINT64_C(0x0000100000000420));
    assert(out.geom_state[0].geom_reg_vdm_context_resume_task0==UINT64_C(0x000010d520001004));
    assert(out.geom_state[0].geom_reg_vdm_context_resume_task1==0x1110);
    assert(out.geom_state[0].geom_reg_vdm_context_resume_task2==UINT64_C(0x0000104000000420));
    assert(!out.geom_reg_vdm_context_state_resume_addr && !out.geom_state[0].geom_reg_vdm_context_store_task3);
    old=out;p.slot[0].pt_store_code+=16;
    assert(a04_render_make_context_static(&p,ranges,2,&out) && !memcmp(&out,&old,sizeof(out)));
    p.slot[0].pt_store_code-=16;p.slot[1]=p.slot[0];
    assert(a04_render_make_context_static(&p,ranges,2,&out) && !memcmp(&out,&old,sizeof(out)));
    return 0;
}
