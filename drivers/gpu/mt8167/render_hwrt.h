/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_RENDER_HWRT_H
#define A04_GE8300_RENDER_HWRT_H
#include <stdint.h>
#include "pvr_rogue_fwif.h"
/* GE8300 SIPF1, 16x16 tile, one sample and one layer only. TPC stride, RTC
 * and macrotile array may be zero for this configuration. All GPU buffers
 * must be independently range-checked, owned and synchronized by the caller. */
struct a04_render_hwrt_params {
    uint32_t width,height,tpc_stride,tpc_size,region_header_size;
    uint32_t merge_lower_x,merge_lower_y,merge_upper_x,merge_upper_y,merge_scale_x,merge_scale_y;
    uint32_t common_fw_address,freelist_fw_address[2];
    uint64_t tpc_gpu,vheap_gpu,rtc_gpu,pm_mlist_gpu,macrotile_gpu,region_gpu;
};
/* On failure leaves both firmware structures unchanged. Does not allocate
 * memory, validate buffer extents, synchronize caches, or submit work. */
int a04_gpu_init_render_hwrt(const struct a04_render_hwrt_params *params,
    struct rogue_fwif_hwrtdata_common *common,struct rogue_fwif_hwrtdata *data);
#endif
