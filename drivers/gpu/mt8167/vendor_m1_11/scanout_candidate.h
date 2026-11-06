/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SCANOUT_CANDIDATE_H
#define A04_SCANOUT_CANDIDATE_H
#include <stdint.h>
#include <stddef.h>
/* This is an offline identity/geometry check, NOT authority to program OVL.
 * pages are CPU physical page bases obtained from the live allocation. The
 * common-buffer logical address belongs to the GPU DMA adapter. Equality does
 * not establish an OVL DMA mapping, GPU cache completion or scanout lifetime. */
static int a04_scanout_candidate_check(uint32_t width, uint32_t height,
    uint32_t pitch, uint64_t gpu_dma, const uint64_t *pages, size_t count)
{
    uint64_t bytes, i;
    if (width != 800u || height != 1280u || pitch != 3200u || !pages)
        return 0;
    bytes = (uint64_t)pitch * height;
    if ((gpu_dma & 4095u) || gpu_dma == 0 || gpu_dma > UINT64_C(0x100000000) - bytes ||
        count != bytes / 4096u)
        return 0;
    for (i = 0; i < count; ++i)
        if (pages[i] != gpu_dma + i * 4096u)
            return 0;
    return 1;
}
#endif
