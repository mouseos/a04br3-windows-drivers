/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_WINDOWS_SURFACE_H
#define A04_VENDOR_WINDOWS_SURFACE_H
#include <stdint.h>
#include "windows_uncached.h"
/* Private GPU-only candidate. Not a WDDM allocation or a scanout surface.
 * The adapter must remain referenced through release. Serialize every call. */
#define A04_SURFACE_MAX_LEAVES 4u
struct a04_vendor_surface {
    struct a04_vendor_uncached pixels,pc,pd,pt[A04_SURFACE_MAX_LEAVES];
    ULONG width,height,pitch,bytes,leaves;
    uint64_t gpu_va;
    BOOLEAN prepared, hardware_exposed;
};
/* Allocate and construct an inactive address space; no register or GPU access.
 * Geometry is native 32-bit linear, pitch/height cover complete pages, maximum
 * four leaves. Logical addresses belong only to this adapter. */
NTSTATUS a04_vendor_surface_prepare(struct a04_vendor_surface *s,PDMA_ADAPTER adapter,
                                   ULONG width,ULONG height,ULONG pitch,uint64_t gpu_va,ULONG map_registers);
/* Call before any submission or publication to hardware; sticky. */
NTSTATUS a04_vendor_surface_mark_exposed(struct a04_vendor_surface *s);
/* Only unexposed objects can currently be freed. GPU stop/scanout ownership is
 * unverified; no boolean from a caller may assert quiescence. */
NTSTATUS a04_vendor_surface_release(struct a04_vendor_surface *s);
#endif
