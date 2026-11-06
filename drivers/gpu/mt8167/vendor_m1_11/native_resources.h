/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_RESOURCES_H
#define A04_M111_NATIVE_RESOURCES_H
#include "A04BR3RogueDDKABI.h"
#define A04_M111_MAX_FREELIST_PAGES 4096u
#define A04_M111_DUMMY_REGION_BYTES 128u
#define A04_M111_DUMMY_STENCIL_BYTES 2048u
struct a04_m111_auxiliary_layout {
    uint64_t dummy_region_gpu,dummy_stencil_gpu,dummy_stencil_register;
    uint32_t used;
};
/* Reserve subranges inside a caller-owned GPU allocation. RGXCreateGlobalPB
 * uses 128 bytes/alignment 64 for DummyRgnHeader; RGXCreateRenderContextCCB
 * uses 2048 bytes/alignment 16 for BRN Dummy Stencil Store. No initial memory
 * contents are inferred here and no allocation/mapping is performed. */
int a04_m111_layout_auxiliary(uint64_t gpu_base,size_t bytes,
    struct a04_m111_auxiliary_layout *out);
/* Cold CPU memory only. The caller maps and retains FW and GPU allocations;
 * these routines do not allocate DMA, publish mappings, or submit commands. */
struct a04_m111_freelist_input {
    uint64_t list_gpu;
    uint32_t pages, id;
};
/* Fully backed, fixed-size list, init=max, grow=0, ready=0. Each supplied
 * physical 4K page is retained by the caller (at most 16MiB per list).
 * Optional reconstruction mirror
 * follows the primary list; capacity is expressed in uint32_t entries. */
int a04_m111_build_freelist(RGXFWIF_FREELIST *out, uint32_t *entries,
    size_t capacity, const struct a04_m111_freelist_input *input,
    const uint64_t *physical_pages, int mirror);
struct a04_m111_hwrt_input {
    uint64_t pm_mlist, tail_ptrs, macrotile, region_header, rtc, region_header_size;
    uint64_t multisample, flipped_multisample;
    uint32_t ppp_screen, tpc_stride, tpc_size, te_screen, te_aa, te_mtile1, te_mtile2;
    uint32_t mtile_stride, merge_lower_x, merge_lower_y, merge_upper_x, merge_upper_y;
    uint32_t merge_scale_x, merge_scale_y, isp_mtile_size;
};
/* MaxRTs=1 only; arrays/shadow-cache storage for MRT are deliberately absent. */
int a04_m111_build_hwrt(RGXFWIF_HWRTDATA *out,
    const struct a04_m111_hwrt_input *input, uint32_t target_fw,
    const uint32_t freelist_fw[2]);
struct a04_m111_native_bank {
    RGXFWIF_RENDER_TARGET target __attribute__((aligned(64)));
    RGXFWIF_FREELIST freelist[2] __attribute__((aligned(64)));
    RGXFWIF_HWRTDATA hwrt[2] __attribute__((aligned(64)));
};
struct a04_m111_native_addresses {
    uint32_t target, freelist[2], hwrt[2], ta_cleanup[2], three_d_cleanup[2];
};
/* Copy fully initialized freelists and build two HWRT slots in a separate FW
 * allocation. fw_base is its mapped MIPS FW address, never a host pointer. */
int a04_m111_build_native_bank(struct a04_m111_native_bank *out,
    struct a04_m111_native_addresses *addresses, uint32_t fw_base,
    uint64_t vheap_gpu, const RGXFWIF_FREELIST freelist[2],
    const struct a04_m111_hwrt_input hwrt[2]);
#endif
