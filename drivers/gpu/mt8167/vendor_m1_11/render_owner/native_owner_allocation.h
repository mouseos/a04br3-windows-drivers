/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OWNER_ALLOCATION_H
#define A04_NATIVE_OWNER_ALLOCATION_H
#include "native_owner_frontend.h"
#include "windows_native_cold.h"
struct a04_native_owner_allocation;
/* Provider-owned cold factory substrate. Real PDO DMA adapter + m1.11 FW,
 * render contexts, HWRT/freelists/code/packet. No WDF or IRQ connection.
 * firmware is a verified kernel-owned file image from the factory loader,
 * never a user payload. It is consumed before return, not retained.
 * frontend may be NULL when factory uses per-resolution bind_lease_cold.
 * SUCCESS here means COLD RESOURCES ONLY, not GeoNativeOwnerCreate/caps.
 * Return nonnull even on error if cleanup could not release real ownership. */
NTSTATUS a04_native_allocation_create(struct a04_native_owner_allocation **,
    PDEVICE_OBJECT,const void *,size_t,const struct a04_native_owner_frontend *);
/* PASSIVE, exclusive factory/lease serialization. Same full469page MDL and
 * actual VidMm aperture offset. One cold binding; no live remap is claimed. */
NTSTATUS a04_native_allocation_bind_cold(struct a04_native_owner_allocation *,
    PMDL,uint64_t aperture_byte_offset,uint32_t packed_argb);
/* H GeoNativeOwnerCreateV3/geo_frontend_memory.resolve adapter: pass the
 * returned independently pinned SAME-PFN lease.mdl and its acquire/release,
 * not the borrowed original VidMm metadata. Native bind acquires its own
 * reference. Caller ALWAYS drops resolve's separate owned reference after
 * this call, on both success and failure. The binding retains its acquire
 * through poison; no copied private target or invented Normal provenance.
 * This bypasses only the factory-wide legacy callback storage, not SG/PFN,
 * MDL/cache/aperture validation or frontend reference acquisition. */
NTSTATUS a04_native_allocation_bind_lease_cold(struct a04_native_owner_allocation *,
    PMDL independently_pinned_mdl,uint64_t aperture_byte_offset,uint32_t packed_argb,
    const struct a04_native_mdl_lease *);
NTSTATUS a04_native_allocation_release_cold(struct a04_native_owner_allocation **);
NTSTATUS a04_native_allocation_aperture(const struct a04_native_owner_allocation *,
    struct a04_native_target_aperture *);
#endif
