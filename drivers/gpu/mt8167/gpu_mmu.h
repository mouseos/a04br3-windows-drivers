/* SPDX-License-Identifier: MIT
 * GE8300 GPU virtual-address page table encoding, based on Imagination's
 * pvr_mmu.c and pvr_rogue_mmu_defs.h (GPL-2.0-only OR MIT). */
#ifndef A04_GE8300_GPU_MMU_H
#define A04_GE8300_GPU_MMU_H
#include <stdint.h>
#include <stddef.h>
#define A04_GPU_PAGE_SIZE 4096u
#define A04_GPU_MMU_READ_ONLY UINT64_C(0x2)
#define A04_GPU_MMU_COHERENT UINT64_C(0x4)
#define A04_GPU_MMU_SLC_BYPASS UINT64_C(0x8)
/* Linux ROGUE_MMUCTRL_PT_DATA_PM_META_PROTECT_EN: restrict GPU access to
 * the Parameter Manager and firmware. Does not restrict CPU access. */
#define A04_GPU_MMU_PM_FW_PROTECT UINT64_C(0x4000000000000000)
/* The caller owns three 4 KiB, zero-initialized DMA-visible table pages.
 * A call maps a physically contiguous, page-aligned range within one 2 MiB
 * leaf table. DMA addresses are the adapter logical addresses, not CPU VAs.
 * The caller serializes access, synchronizes all changed tables to the GPU,
 * and invalidates the GPU MMU before using or reusing any mapping. Never
 * alter or free a live mapping until in-flight GPU work has been quiesced. */
int a04_gpu_mmu_map_4k(uint32_t *pc,uint64_t *pd,uint64_t *pt,
    uint64_t pd_dma,uint64_t pt_dma,uint64_t gpu_va,uint64_t dma,
    size_t page_count,uint64_t flags);
/* As above, but each GPU page uses a separately supplied DMA address.
 * The DMA-address array must not overlap any destination page table. */
int a04_gpu_mmu_map_pages_4k(uint32_t *pc,uint64_t *pd,uint64_t *pt,
    uint64_t pd_dma,uint64_t pt_dma,uint64_t gpu_va,const uint64_t *pages,
    size_t page_count,uint64_t flags);
/* Replace an entirely present range in one leaf table with adapter DMA pages.
 * All entries and input addresses are validated before any are changed.
 * The caller serializes access, waits for all jobs using the old mapping,
 * cleans the changed table and waits for GPU MMU invalidation; retain both
 * old and new DMA mappings until the invalidate succeeds. The input array
 * must not overlap the leaf table. This does not acquire or pin DMA pages. */
int a04_gpu_mmu_replace_pages_4k(uint64_t *pt,uint64_t gpu_va,
    const uint64_t *pages,size_t page_count,uint64_t flags);
/* Quiesce GPU jobs before unmapping. The caller synchronizes the changed
 * table and waits for MMU invalidation before freeing or reusing memory. */
int a04_gpu_mmu_unmap_4k(uint64_t *pt,uint64_t gpu_va,size_t page_count);
/* HWBRN65273: map the same zeroed DMA page at both reserved addresses.
 * The caller must zero and synchronize the page and all five table pages,
 * then wait for MMU invalidation before using this context. */
int a04_gpu_mmu_map_brn65273(uint32_t *pc,uint64_t *pd_a,uint64_t *pt_a,
    uint64_t *pd_b,uint64_t *pt_b,uint64_t pd_a_dma,uint64_t pt_a_dma,
    uint64_t pd_b_dma,uint64_t pt_b_dma,uint64_t zero_dma);
#endif
