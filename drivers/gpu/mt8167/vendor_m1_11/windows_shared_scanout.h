/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_SHARED_SCANOUT_H
#define A04_WINDOWS_SHARED_SCANOUT_H
#include <ntddk.h>
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define A04_SHARED_SCANOUT_BYTES 4096000u
#define A04_SHARED_SCANOUT_PAGES 1000u
#ifndef A04_GE8300_SHARED_DISPLAY_COMMON
#define A04_GE8300_SHARED_DISPLAY_COMMON 0
#endif
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
    /* Owned externally; never fabricate an SG list for a common buffer. */
    BOOLEAN display_common;
};
static __inline int a04_shared_has_display_backing(const struct a04_shared_scanout *s)
{
    return s && (s->display_common ?
        (A04_GE8300_SHARED_DISPLAY_COMMON && !s->display_sg && s->display_adapter && s->display_dma) : s->display_sg != NULL);
}
#if defined(_ARM_)
#ifdef __cplusplus
static_assert(sizeof(struct a04_shared_scanout)==32 && offsetof(struct a04_shared_scanout,gpu_pages)==20 && offsetof(struct a04_shared_scanout,exposed)==28, "shared scanout ABI");
#else
_Static_assert(sizeof(struct a04_shared_scanout)==32 && offsetof(struct a04_shared_scanout,gpu_pages)==20 && offsetof(struct a04_shared_scanout,exposed)==28, "shared scanout ABI");
#endif
#endif
/* Current native owner variant: GPU PTEs use retained VidMm physical PFNs,
 * not a HAL packet transfer. Match the DIS0 SG mapping page-for-page to the
 * SAME Normal-RAM MDL and require the display's actual contiguous span. */
NTSTATUS a04_shared_scanout_match_vidmm(PMDL,PSCATTER_GATHER_LIST,uint32_t *display_dma);
NTSTATUS a04_shared_scanout_prepare(struct a04_shared_scanout *,
    PDMA_ADAPTER gpu,PDEVICE_OBJECT gpu_pdo,
    PDMA_ADAPTER display,PDEVICE_OBJECT display_pdo);
/* No hardware may use the mappings before a separately implemented lifetime
 * and visibility protocol; an exposed allocation can never be freed here. */
NTSTATUS a04_shared_scanout_release(struct a04_shared_scanout *);
/* One-way handoff. The caller serializes lifetime and proves GPU root removal,
 * MMU invalidation, GPU DMA drain and that OVL cannot read either mapping.
 * There is currently no production caller capable of proving these facts.
 * Failure after Put retains the allocation and never restores GPU access. */
typedef NTSTATUS (*a04_shared_quiesce_proof)(void *);
NTSTATUS a04_shared_scanout_handoff_display(struct a04_shared_scanout *,
    PDEVICE_OBJECT display_pdo,a04_shared_quiesce_proof,void *);
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
