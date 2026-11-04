/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_RENDER_FREELIST_H
#define A04_GE8300_RENDER_FREELIST_H
#include <stddef.h>
#include <stdint.h>
#include "pvr_rogue_fwif.h"
/* Construct a fixed-size, non-growing PM free list from device DMA addresses.
 * The caller owns and retains every 4 KiB DMA page and the GPU-mapped table,
 * ensures there are no overlaps (including between the supplied buffers), and
 * synchronizes the CPU writes before use.
 * This does not create a render context, HWRT, scheduler, or validate GPU use. */
int a04_gpu_init_render_freelist(uint32_t *table,size_t table_entries,
    uint64_t table_gpu_address,const uint64_t *page_dma,size_t page_count,
    uint32_t freelist_id,struct rogue_fwif_freelist *firmware);
/* As above, but PM pages are contiguous adapter DMA addresses. The caller
 * independently proves exclusive ownership and non-overlap with mappings. */
int a04_gpu_init_render_freelist_contiguous(uint32_t *table,size_t table_entries,
    uint64_t table_gpu_address,uint64_t first_dma,size_t page_count,
    uint32_t freelist_id,struct rogue_fwif_freelist *firmware);
#endif
