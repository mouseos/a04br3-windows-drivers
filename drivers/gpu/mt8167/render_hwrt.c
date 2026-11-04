/* SPDX-License-Identifier: MIT
 * Single-sample SIPF1 layout from Linux pvr_hwrt.c, Copyright (c) 2023
 * Imagination Technologies Ltd., MIT option. No kernel memory mapping here.
 */
#include "render_hwrt.h"
#define GPU_ADDR_LIMIT UINT64_C(0x10000000000)
static int gpu_pointer(uint64_t value) { return value && value<GPU_ADDR_LIMIT && !(value&15u); }
static int fw_pointer(uint32_t value) { return value && !(value&63u); }
int a04_gpu_init_render_hwrt(const struct a04_render_hwrt_params *p,
    struct rogue_fwif_hwrtdata_common *common,struct rogue_fwif_hwrtdata *data)
{
    struct rogue_fwif_hwrtdata_common c={0}; struct rogue_fwif_hwrtdata d={0};
    uint32_t x,y;
    if (!p || !common || !data || !p->width || !p->height ||
        p->width>8192u || p->height>8192u || !p->tpc_stride || !p->tpc_size ||
        !p->region_header_size || !fw_pointer(p->common_fw_address) ||
        !fw_pointer(p->freelist_fw_address[0]) || !fw_pointer(p->freelist_fw_address[1]) ||
        !gpu_pointer(p->tpc_gpu) || !gpu_pointer(p->vheap_gpu) || !gpu_pointer(p->rtc_gpu) ||
        !gpu_pointer(p->pm_mlist_gpu) || !gpu_pointer(p->macrotile_gpu) || !gpu_pointer(p->region_gpu)) return -1;
    x=(p->width+15u)/16u; y=(p->height+15u)/16u;
    if (x>511u || y>511u) return -1;
    c.screen_pixel_max=((p->height-1u)<<16)|(p->width-1u);
    c.multi_sample_ctl=0x88u; c.flipped_multi_sample_ctl=0x88u;
    c.tpc_stride=p->tpc_stride; c.tpc_size=p->tpc_size;
    c.te_screen=((y-1u)<<12)|(x-1u); c.mtile_stride=x*y;
    c.te_mtile1=x<<18; c.te_mtile2=y<<18;
    c.isp_merge_lower_x=p->merge_lower_x; c.isp_merge_lower_y=p->merge_lower_y;
    c.isp_merge_upper_x=p->merge_upper_x; c.isp_merge_upper_y=p->merge_upper_y;
    c.isp_merge_scale_x=p->merge_scale_x; c.isp_merge_scale_y=p->merge_scale_y;
    c.rgn_header_size=p->region_header_size; c.isp_mtile_size=(x<<16)|y;
    d.hwrt_data_common_fw_addr=p->common_fw_address;
    d.freelists_fw_addr[0]=p->freelist_fw_address[0]; d.freelists_fw_addr[1]=p->freelist_fw_address[1];
    d.tail_ptrs_dev_addr=p->tpc_gpu; d.vheap_table_dev_addr=p->vheap_gpu; d.rtc_dev_addr=p->rtc_gpu;
    d.pm_mlist_dev_addr=p->pm_mlist_gpu; d.macrotile_array_dev_addr=p->macrotile_gpu;
    d.rgn_header_dev_addr=p->region_gpu; d.rta_ctl.max_rts=1;
    *common=c; *data=d; return 0;
}
