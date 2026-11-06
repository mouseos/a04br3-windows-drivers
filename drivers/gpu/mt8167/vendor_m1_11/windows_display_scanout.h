/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_DISPLAY_SCANOUT_H
#define A04_WINDOWS_DISPLAY_SCANOUT_H
#include <ntddk.h>
#include "windows_surface.h"
#ifndef A04_GE8300_DISPLAY_SCANOUT_EXPERIMENT
#define A04_GE8300_DISPLAY_SCANOUT_EXPERIMENT 0
#endif
#if A04_GE8300_DISPLAY_SCANOUT_EXPERIMENT
/* All calls are serialized at PASSIVE_LEVEL. The owner retains the GPU
 * surface, locked MDL, both adapters, PDO and exclusive PnP OVL resource.
 * The mapping and all owners remain alive forever after an attempted switch;
 * this interface has no unproven display/GPU quiescence escape hatch. */
struct a04_display_scanout {
    struct a04_vendor_surface *surface;
    PDMA_ADAPTER display_adapter;
    PMDL mdl;
    PSCATTER_GATHER_LIST sg;
    uint32_t display_address, saved_address;
    volatile ULONG *ovl;
    ULONG ovl_length;
    BOOLEAN attempted;
};
/* MDL describes the complete page-aligned GPU allocation and is locked by
 * its owner. Display adapter must be for OVL, not the GPU PDO. */
NTSTATUS a04_display_scanout_map(struct a04_display_scanout *,
    struct a04_vendor_surface *, PDMA_ADAPTER, PDEVICE_OBJECT, PMDL);
/* Safe only before an OVL write is attempted. */
NTSTATUS a04_display_scanout_unmap(struct a04_display_scanout *);
/* callbacks are trusted platform operations, not caller-supplied flags:
 * ready waits the exact render fence and completes GPU-to-OVL visibility,
 * including any DMA bounce-buffer synchronization;
 * boundary confirms a safe frame boundary while the caller holds an exclusive
 * display lease across the entire call.  The
 * current implementation supplies neither: no hardware path is registered. */
typedef NTSTATUS (*a04_display_ready_fn)(void *context, uint64_t fence);
typedef NTSTATUS (*a04_display_boundary_fn)(void *context);
NTSTATUS a04_display_scanout_switch(struct a04_display_scanout *,
    volatile void *ovl, ULONG length, uint32_t expected_old_address,
    uint64_t fence, a04_display_ready_fn ready,
    a04_display_boundary_fn boundary, void *context);
#endif
#endif
