/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_M111_WINDOWS_NATIVE_COLD_H
#define MT8167_M111_WINDOWS_NATIVE_COLD_H
#include "windows_render_cold.h"
#include "native_resources.h"
#define A04_NATIVE_DATA_VA UINT64_C(0x65c0200000)
#define A04_NATIVE_DATA_LIMIT 0x200000u
struct a04_vendor_native_cold {
    struct a04_vendor_render_cold *render;
    struct a04_vendor_uncached data,pt;
    struct a04_m111_target_storage target;
    struct a04_m111_mlist_storage mlists;
    struct a04_m111_auxiliary_layout auxiliary;
    RGXFWIF_FREELIST freelist[2];
    struct a04_m111_native_addresses addresses;
    uint64_t pm_pages[296]; /* local 40 + global 256, adapter logical addresses */
    ULONG width,height,pm_offset[2],list_offset[2],used;
    BOOLEAN mapped,prepared,bank_prepared,bank_touched;
};
/* Cold backing only. Complete native records/code are deliberately separate.
 * Called under the render owner's startup serialization before FW execution.
 * All resources remain retained once firmware.hardware_owned becomes true. */
NTSTATUS a04_vendor_native_prepare_cold(struct a04_vendor_native_cold *,
    struct a04_vendor_render_cold *,PDMA_ADAPTER,ULONG map_registers,
    ULONG width,ULONG height);
/* The caller supplies independently derived HWRT register values. This binds
 * the native FW bank, not a complete TA/3D payload or execution permission. */
NTSTATUS a04_vendor_native_bind_bank_cold(struct a04_vendor_native_cold *,
    const struct a04_m111_hwrt_input hwrt[2]);
NTSTATUS a04_vendor_native_release_cold(struct a04_vendor_native_cold *);
#endif
