/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_WINDOWS_COPY_SCRATCH_H
#define A04_VENDOR_WINDOWS_COPY_SCRATCH_H
#include "windows_render_cold.h"
#include "native_copy_state.h"
/* Caller serializes all lifecycle operations and retains the adapter, render,
 * firmware and this owner. map_registers is IoGetDmaAdapter output.
 * A prepared child blocks render/firmware release; exposure is sticky.
 * Nothing here authorizes a GPU command or proves PnP removal safety. */
#define A04_VENDOR_COPY_SOURCE_VA (A04_VENDOR_RENDER_SCRATCH_VA+UINT64_C(0x10000))
#define A04_VENDOR_COPY_DEST_VA   (A04_VENDOR_RENDER_SCRATCH_VA+UINT64_C(0x11000))
#define A04_VENDOR_COPY_SHARED_VA (A04_VENDOR_RENDER_SCRATCH_VA+UINT64_C(0x12000))
struct a04_vendor_copy_scratch {
    struct a04_vendor_uncached source,destination,shared;
    struct a04_vendor_render_cold *render;
    struct a04_vendor_copy_state state;
    BOOLEAN prepared,exposed;
};
NTSTATUS a04_vendor_copy_scratch_prepare(struct a04_vendor_copy_scratch *owner,
    struct a04_vendor_render_cold *render,ULONG map_registers);
NTSTATUS a04_vendor_copy_scratch_mark_exposed(struct a04_vendor_copy_scratch *owner);
NTSTATUS a04_vendor_copy_scratch_release(struct a04_vendor_copy_scratch *owner);
#endif
