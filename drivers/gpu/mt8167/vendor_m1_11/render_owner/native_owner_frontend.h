/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OWNER_FRONTEND_H
#define A04_NATIVE_OWNER_FRONTEND_H
#include "native_owner_mdl.h"
#define A04_NATIVE_FRONTEND_VERSION 1u
/* Source-linked factory input from the render miniport, not an IOCTL or
 * arbitrary caller assertion. H owns these two callbacks and their registry.
 * acquire atomically resolves THIS exact MDL in the frontend's live VidMm
 * allocation registry, verifies its actual Normal-RAM allocation origin and
 * locked pages, and retains that allocation/pages until release. A flag on
 * an unknown MDL, pool/common-buffer alias or a reconstructed PFN list is not
 * sufficient. On failure no reference is taken. release drops precisely the
 * successful acquire and never unlocks/frees VidMm pages behind the frontend.
 * context and callback code remain resident through every retained binding,
 * including poison/removal. Provider supplies real SG/cache/GPU mapping. */
struct a04_native_owner_frontend {
    ULONG size,version;
    struct a04_native_mdl_lease normal_mdl;
};
#endif
