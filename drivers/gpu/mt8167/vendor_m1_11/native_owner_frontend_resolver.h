/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OWNER_FRONTEND_RESOLVER_H
#define A04_NATIVE_OWNER_FRONTEND_RESOLVER_H
#include "native_owner_allocation.h"
#include "../wddm-render/frontend_memory.h"
/* H fixed frontend ABI f492756. Provider implements the resolve -> native
 * acquire -> exact-SG bind -> release-resolve-reference transaction. H keeps
 * its actual allocation/independent-PFN-pin registry and final factory call.
 * PASSIVE and existing factory/owner lifecycle serialization required.
 * This is a bind operation, not a complete CreateV3/control/capability proof. */
NTSTATUS a04_native_allocation_bind_frontend_cold(struct a04_native_owner_allocation *,
    const struct geo_frontend_memory *,PMDL original_vidmm_mdl,ULONG active_bytes,
    uint64_t aperture_byte_offset,uint32_t packed_argb);
#endif
