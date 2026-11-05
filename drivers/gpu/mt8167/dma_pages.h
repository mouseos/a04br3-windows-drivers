/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_DMA_PAGES_H
#define A04_GE8300_DMA_PAGES_H
#include <stddef.h>
#include <stdint.h>
struct a04_gpu_dma_segment { uint64_t address; uint64_t length; };
/* Convert an OS-provided DMA scatter/gather list to whole 4 KiB device pages.
 * A segment may split a page only if the next segment continues at the same
 * DMA address. Input must cover exactly page_count pages (up to 8192). No output is written
 * on failure, and output must not overlap the input list. The caller must
 * keep the input list unchanged throughout the call. This does not map,
 * pin, synchronize or release memory; the OS DMA mappings must remain owned
 * until GPU completion and MMU invalidation. */
int a04_gpu_dma_pages(const struct a04_gpu_dma_segment *segments,size_t count,
    uint64_t *pages,size_t page_count);
#endif
