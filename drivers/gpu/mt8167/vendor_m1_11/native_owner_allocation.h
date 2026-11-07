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
 * SUCCESS here means COLD RESOURCES ONLY, not GeoNativeOwnerCreate/caps.
 * Return nonnull even on error if cleanup could not release real ownership. */
NTSTATUS a04_native_allocation_create(struct a04_native_owner_allocation **,
    PDEVICE_OBJECT,const void *,size_t,const struct a04_native_owner_frontend *);
/* PASSIVE, exclusive factory/lease serialization. Same full469page MDL and
 * actual VidMm aperture offset. One cold binding; no live remap is claimed. */
NTSTATUS a04_native_allocation_bind_cold(struct a04_native_owner_allocation *,
    PMDL,uint64_t aperture_byte_offset,uint32_t packed_argb);
NTSTATUS a04_native_allocation_release_cold(struct a04_native_owner_allocation **);
NTSTATUS a04_native_allocation_aperture(const struct a04_native_owner_allocation *,
    struct a04_native_target_aperture *);
#endif
