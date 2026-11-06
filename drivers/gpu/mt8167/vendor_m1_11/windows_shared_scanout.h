/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_SHARED_SCANOUT_H
#define A04_WINDOWS_SHARED_SCANOUT_H
#include <ntddk.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define A04_SHARED_SCANOUT_BYTES 4096000u
#define A04_SHARED_SCANOUT_PAGES 1000u
/* Serialized at PASSIVE_LEVEL. Both PDOs and adapters remain referenced by
 * their respective owners. Pages and two DMA mappings share ONE OS MDL.
 * Preparation alone does not grant hardware access or prove no bounce. */
struct a04_shared_scanout {
    PMDL mdl;
    PDMA_ADAPTER gpu_adapter,display_adapter;
    PSCATTER_GATHER_LIST gpu_sg,display_sg;
    uint64_t *gpu_pages;
    uint32_t display_dma;
    BOOLEAN exposed;
};
NTSTATUS a04_shared_scanout_prepare(struct a04_shared_scanout *,
    PDMA_ADAPTER gpu,PDEVICE_OBJECT gpu_pdo,
    PDMA_ADAPTER display,PDEVICE_OBJECT display_pdo);
/* No hardware may use the mappings before a separately implemented lifetime
 * and visibility protocol; an exposed allocation can never be freed here. */
NTSTATUS a04_shared_scanout_release(struct a04_shared_scanout *);
/* Build inactive GPU page-table images for the 1000 mapped pages. The GPU
 * owner provides independent GPU-DMA table addresses and later publishes the
 * images; this function does not acquire table memory or invalidate the MMU. */
NTSTATUS a04_shared_scanout_build_tables(const struct a04_shared_scanout *,
    uint32_t *pc,uint64_t pc_dma,uint64_t *pd,uint64_t pd_dma,
    uint64_t *pt0,uint64_t pt0_dma,uint64_t *pt1,uint64_t pt1_dma);
#ifdef __cplusplus
}
#endif
#endif
