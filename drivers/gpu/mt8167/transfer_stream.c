/* SPDX-License-Identifier: MIT
 * Transfer layout and ordering from Imagination's Linux pvr_stream_defs.c and
 * pvr_stream.c, Copyright (c) 2023 Imagination Technologies Ltd., MIT option. */
#include "transfer_stream.h"
#include <stddef.h>
struct field { size_t offset,size,align; uint32_t feature; };
#define F(name,bytes) {offsetof(struct rogue_fwif_cmd_transfer,regs.name),bytes,bytes,0}
#define O(name,bit) {offsetof(struct rogue_fwif_cmd_transfer,regs.name),4,4,bit}
static const struct field fields[]={
    F(pds_bgnd0_base,8),F(pds_bgnd1_base,8),F(pds_bgnd3_sizeinfo,8),F(isp_mtile_base,8),
    {offsetof(struct rogue_fwif_cmd_transfer,regs.pbe_wordx_mrty),sizeof(((struct rogue_fwif_cmd_transfer *)0)->regs.pbe_wordx_mrty),8,0},
    F(isp_bgobjvals,4),F(usc_pixel_output_ctrl,4),F(usc_clear_register0,4),F(usc_clear_register1,4),
    F(usc_clear_register2,4),F(usc_clear_register3,4),F(isp_mtile_size,4),F(isp_render_origin,4),
    F(isp_ctl,4),F(isp_aa,4),F(event_pixel_pds_info,4),F(event_pixel_pds_code,4),
    F(event_pixel_pds_data,4),F(isp_render,4),F(isp_rgn,4),
    O(isp_xtp_pipe_enable,A04_TRANSFER_S7_TOP),O(frag_screen,A04_TRANSFER_MULTICORE)
};
static void copy(void *dst,const void *src,size_t n)
{ size_t i; for(i=0;i<n;++i) ((uint8_t *)dst)[i]=((const uint8_t *)src)[i]; }
int a04_gpu_decode_transfer(const void *stream,size_t size,uint32_t features,struct rogue_fwif_cmd_transfer *out)
{
    const uint8_t *bytes=stream; struct rogue_fwif_cmd_transfer result={0};
    uint32_t main_size,padding; size_t i,offset=8;
    if (!stream || !out || size<8 || size>UINT32_MAX ||
        (features&~(A04_TRANSFER_S7_TOP|A04_TRANSFER_MULTICORE))) return -1;
    copy(&main_size,bytes,4); copy(&padding,bytes+4,4);
    if (padding || main_size!=size) return -1;
    for(i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
        const struct field *f=&fields[i]; size_t aligned;
        if (f->feature && !(features&f->feature)) continue;
        if (offset>size || f->align-1>size-offset) return -1;
        aligned=(offset+f->align-1)&~(f->align-1);
        if (aligned>size || f->size>size-aligned || f->offset>sizeof(result) || f->size>sizeof(result)-f->offset) return -1;
        copy((uint8_t *)&result+f->offset,bytes+aligned,f->size); offset=aligned+f->size;
    }
    if (offset!=size) return -1;
    *out=result; return 0;
}
