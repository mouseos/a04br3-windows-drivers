/* SPDX-License-Identifier: MIT
 * Fixed registers from Mesa ef11349e8e and Linux pvr_job.c flag conversion. */
#include "render_fixed_command.h"
static int owns(const struct a04_render_owned_range *r,size_t count,uint64_t a,uint64_t n,int write)
{
    size_t i;unsigned matches=0;
    for(i=0;i<count;++i) if ((r[i].access&A04_RENDER_RANGE_READ) &&
        !!(r[i].access&A04_RENDER_RANGE_WRITE)==!!write && a>=r[i].gpu_va && n<=r[i].size && a-r[i].gpu_va<=r[i].size-n) ++matches;
    return matches==1;
}
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
int a04_render_make_fixed_commands(const struct a04_render_fixed_command_input *p,
    const struct a04_render_owned_range *r,size_t count,struct rogue_fwif_cmd_geom *geom,struct rogue_fwif_cmd_frag *frag)
{
    struct rogue_fwif_cmd_geom g={0};struct rogue_fwif_cmd_frag f={0};
    uint64_t a[9],n[9]={60,262144,8,16384,48,16,16,16,16};size_t i,j;
    if (!p || !geom || !frag || a04_render_validate_owned_ranges(r,count)) return -1;
    a[0]=p->vdm;a[1]=p->border;a[2]=p->scissor;a[3]=p->target;a[4]=p->background_shader;
    a[5]=p->background_code;a[6]=p->background_data;a[7]=p->partial_background_data;a[8]=p->event_data;
    if ((a[0]&3u) || (a[1]&63u) || (a[2]&3u) || (a[3]&4095u)) return -1;
    for(i=0;i<9;++i) {
        if (i>=4 && ((a[i]>>32)!=UINT64_C(0xda) || (a[i]&15u))) return -1;
        if (!owns(r,count,a[i],n[i],i==3)) return -1;
        for(j=0;j<i;++j) if(overlap(a[i],n[i],a[j],n[j]) && !(i==7 && j==6 && a[i]==a[j])) return -1;
    }
    g.regs.vdm_ctrl_stream_base=p->vdm;g.regs.tpu_border_colour_table=p->border>>2;
    g.regs.ppp_ctrl=UINT32_C(0x202);g.regs.te_psg=UINT32_C(0x20000);
    g.regs.vdm_context_resume_task0_size=UINT32_C(0x20010000);g.regs.tpu=UINT32_C(0x20);
    g.flags=UINT32_C(3);
    f.regs.isp_scissor_base=p->scissor;f.regs.pbe_word[0][0]=UINT64_C(0x0200007c68800000);
    f.regs.pbe_word[0][1]=UINT64_C(0x0000003f0000003f);f.regs.tpu_border_colour_table=p->border>>2;
    f.regs.pds_bgnd[0]=((uint64_t)(uint32_t)p->background_code<<32)|(uint32_t)p->background_shader;
    f.regs.pds_bgnd[1]=(uint64_t)(uint32_t)p->background_data<<32;
    f.regs.pds_bgnd[2]=UINT64_C(0x0080000000010000);
    f.regs.pds_pr_bgnd[0]=f.regs.pds_bgnd[0];f.regs.pds_pr_bgnd[1]=(uint64_t)(uint32_t)p->partial_background_data<<32;
    f.regs.pds_pr_bgnd[2]=f.regs.pds_bgnd[2];
    f.regs.usc_pixel_output_ctrl=UINT32_C(0x1ffc);f.regs.isp_bgobjdepth=UINT32_C(0x3f800000);
    f.regs.isp_bgobjvals=UINT32_C(0x3ff);f.regs.isp_ctl=UINT32_C(0x33000);
    f.regs.event_pixel_pds_info=UINT32_C(0x201);f.regs.event_pixel_pds_data=(uint32_t)p->event_data;
    f.regs.tpu=UINT32_C(0x20);f.flags=UINT32_C(0x8000);
    *geom=g;*frag=f;return 0;
}
