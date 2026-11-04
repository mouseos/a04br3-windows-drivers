/* SPDX-License-Identifier: MIT
 * Stream layouts based on Linux pvr_stream_defs.c and pvr_stream.c,
 * Copyright (c) 2023 Imagination Technologies Ltd., MIT option.
 */
#include "render_stream.h"
#include <stddef.h>
#define ALL_CAPS ((1u << 10) - 1u)
struct field { size_t offset, size, align; uint32_t capability; };
#define F(type,member,n) {offsetof(struct type,member),n,n,0}
#define O(type,member,n,cap) {offsetof(struct type,member),n,n,cap}
#define ARRAY(type,member) {offsetof(struct type,member),sizeof(((struct type *)0)->member),8,0}
static const struct field geometry[] = {
    F(rogue_fwif_cmd_geom,regs.vdm_ctrl_stream_base,8),
    F(rogue_fwif_cmd_geom,regs.tpu_border_colour_table,8),
    O(rogue_fwif_cmd_geom,regs.vdm_draw_indirect0,8,A04_RENDER_DRAW_INDIRECT),
    O(rogue_fwif_cmd_geom,regs.vdm_draw_indirect1,4,A04_RENDER_DRAW_INDIRECT),
    F(rogue_fwif_cmd_geom,regs.ppp_ctrl,4), F(rogue_fwif_cmd_geom,regs.te_psg,4),
    F(rogue_fwif_cmd_geom,regs.vdm_context_resume_task0_size,4),
    O(rogue_fwif_cmd_geom,regs.vdm_context_resume_task3_size,4,A04_RENDER_OBJECT_LLS),
    F(rogue_fwif_cmd_geom,regs.view_idx,4),
    O(rogue_fwif_cmd_geom,regs.pds_coeff_free_prog,4,A04_RENDER_TESSELLATION),
};
static const struct field fragment[] = {
    F(rogue_fwif_cmd_frag,regs.isp_scissor_base,8), F(rogue_fwif_cmd_frag,regs.isp_dbias_base,8),
    F(rogue_fwif_cmd_frag,regs.isp_oclqry_base,8), F(rogue_fwif_cmd_frag,regs.isp_zlsctl,8),
    F(rogue_fwif_cmd_frag,regs.isp_zload_store_base,8), F(rogue_fwif_cmd_frag,regs.isp_stencil_load_store_base,8),
    O(rogue_fwif_cmd_frag,regs.fb_cdc_zls,8,A04_RENDER_FB_CDC_ZLS),
    ARRAY(rogue_fwif_cmd_frag,regs.pbe_word), F(rogue_fwif_cmd_frag,regs.tpu_border_colour_table,8),
    ARRAY(rogue_fwif_cmd_frag,regs.pds_bgnd), ARRAY(rogue_fwif_cmd_frag,regs.pds_pr_bgnd),
    ARRAY(rogue_fwif_cmd_frag,regs.usc_clear_register), F(rogue_fwif_cmd_frag,regs.usc_pixel_output_ctrl,4),
    F(rogue_fwif_cmd_frag,regs.isp_bgobjdepth,4), F(rogue_fwif_cmd_frag,regs.isp_bgobjvals,4),
    F(rogue_fwif_cmd_frag,regs.isp_aa,4), O(rogue_fwif_cmd_frag,regs.isp_xtp_pipe_enable,4,A04_RENDER_S7_TOP),
    F(rogue_fwif_cmd_frag,regs.isp_ctl,4), F(rogue_fwif_cmd_frag,regs.event_pixel_pds_info,4),
    O(rogue_fwif_cmd_frag,regs.pixel_phantom,4,A04_RENDER_CLUSTER_GROUPING),
    F(rogue_fwif_cmd_frag,regs.view_idx,4), F(rogue_fwif_cmd_frag,regs.event_pixel_pds_data,4),
    O(rogue_fwif_cmd_frag,regs.isp_oclqry_stride,4,A04_RENDER_MULTICORE),
    O(rogue_fwif_cmd_frag,regs.isp_zls_pixels,4,A04_RENDER_ZLS_SUBTILE),
    O(rogue_fwif_cmd_frag,regs.rgx_cr_blackpearl_fix,4,A04_RENDER_ZLS_D24S8),
    F(rogue_fwif_cmd_frag,zls_stride,4), F(rogue_fwif_cmd_frag,sls_stride,4),
    O(rogue_fwif_cmd_frag,execute_count,4,A04_RENDER_MULTICORE),
};
static void copy(void *dst,const void *src,size_t n)
{ size_t i; for (i=0;i<n;i++) ((uint8_t *)dst)[i]=((const uint8_t *)src)[i]; }
static int decode(const void *input,size_t size,uint32_t caps,const struct field *fields,size_t count,
                  void *out,size_t out_size,size_t quirk_offset,uint32_t quirk_header)
{
    const uint8_t *bytes=input; uint32_t main_size,pad,ext; size_t i,off=8;
    if (!input || !out || size<8 || size>UINT32_MAX || (caps&~ALL_CAPS)) return -1;
    copy(&main_size,bytes,4); copy(&pad,bytes+4,4);
    if (pad || main_size<8 || main_size>size) return -1;
    for (i=0;i<count;i++) {
        const struct field *f=&fields[i]; size_t aligned;
        if (f->capability && !(caps&f->capability)) continue;
        if (off>main_size || f->align-1>main_size-off) return -1;
        aligned=(off+f->align-1)&~(f->align-1);
        if (aligned>main_size || f->size>main_size-aligned || f->offset>out_size || f->size>out_size-f->offset) return -1;
        copy((uint8_t *)out+f->offset,bytes+aligned,f->size); off=aligned+f->size;
    }
    if (off!=main_size) return -1;
    if (caps&A04_RENDER_QUIRK_49927) {
        if (size-main_size!=8 || quirk_offset>out_size || 4>out_size-quirk_offset) return -1;
        copy(&ext,bytes+main_size,4);
        if (ext!=quirk_header) return -1;
        copy((uint8_t *)out+quirk_offset,bytes+main_size+4,4);
    } else if (size!=main_size) return -1;
    return 0;
}
int a04_gpu_decode_geometry(const void *stream,size_t size,uint32_t caps,struct rogue_fwif_cmd_geom *out)
{
    struct rogue_fwif_cmd_geom result={0};
    if (!out || decode(stream,size,caps,geometry,sizeof(geometry)/sizeof(geometry[0]),&result,sizeof(result),
                       offsetof(struct rogue_fwif_cmd_geom,regs.tpu),1)) return -1;
    *out=result; return 0;
}
int a04_gpu_decode_fragment(const void *stream,size_t size,uint32_t caps,struct rogue_fwif_cmd_frag *out)
{
    struct rogue_fwif_cmd_frag result={0};
    if (!out || decode(stream,size,caps,fragment,sizeof(fragment)/sizeof(fragment[0]),&result,sizeof(result),
                       offsetof(struct rogue_fwif_cmd_frag,regs.tpu),2)) return -1;
    *out=result; return 0;
}
