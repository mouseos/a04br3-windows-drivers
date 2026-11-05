/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_fixed_command.h"
int main(void)
{
    struct a04_render_owned_range r[]={
      {0x8000000000ULL,0x100000,262144,1}, {0x8100000000ULL,0x200000,4096,1},
      {0xda00001000ULL,0x201000,4096,1}, {0x8200000000ULL,0x202000,16384,3}};
    struct a04_render_fixed_command_input p={0};
    struct rogue_fwif_cmd_geom g,oldg;struct rogue_fwif_cmd_frag f,oldf;
    p.border=0x8000000000ULL;p.vdm=0x8100000000ULL;p.scissor=p.vdm+0x100;p.target=0x8200000000ULL;
    p.background_shader=0xda00001100ULL;p.background_code=0xda00001140ULL;p.background_data=0xda00001150ULL;
    p.partial_background_data=0xda00001160ULL;p.event_data=0xda00001170ULL;
    assert(!a04_render_make_fixed_commands(&p,r,4,&g,&f));
    assert(g.flags==3 && g.regs.vdm_ctrl_stream_base==p.vdm && g.regs.tpu_border_colour_table==(p.border>>2));
    assert(g.regs.vdm_context_resume_task0_size==0x20010000 && g.regs.tpu==0x20);
    assert(f.flags==UINT32_C(0x8000) && f.regs.isp_scissor_base==p.scissor);
    assert(f.regs.pbe_word[0][0]==UINT64_C(0x0200007c68800000) && f.regs.pbe_word[0][1]==UINT64_C(0x0000003f0000003f));
    assert(f.regs.pds_bgnd[0]==UINT64_C(0x0000114000001100) && f.regs.pds_bgnd[1]==UINT64_C(0x0000115000000000));
    assert(f.regs.pds_pr_bgnd[1]==UINT64_C(0x0000116000000000) && f.regs.event_pixel_pds_data==0x1170);
    oldg=g;oldf=f;p.border+=64;
    assert(a04_render_make_fixed_commands(&p,r,4,&g,&f) && !memcmp(&g,&oldg,sizeof(g)) && !memcmp(&f,&oldf,sizeof(f)));
    p.border-=64;p.background_data=p.event_data;
    assert(a04_render_make_fixed_commands(&p,r,4,&g,&f) && !memcmp(&g,&oldg,sizeof(g)) && !memcmp(&f,&oldf,sizeof(f)));
    p.background_data=0xda00001150ULL;p.partial_background_data=p.background_data;
    assert(!a04_render_make_fixed_commands(&p,r,4,&g,&f) && f.regs.pds_pr_bgnd[1]==f.regs.pds_bgnd[1]);
    return 0;
}
