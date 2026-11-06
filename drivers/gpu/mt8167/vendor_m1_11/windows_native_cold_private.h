/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_M111_WINDOWS_NATIVE_COLD_PRIVATE_H
#define MT8167_M111_WINDOWS_NATIVE_COLD_PRIVATE_H
#include "windows_native_cold.h"
#include "windows_render_cold.h"
#include "native_resources.h"
#include "native_clear_surface.h"
#include "native_clear_codespace.h"
#define A04_NATIVE_OUTPUT_VA UINT64_C(0x8200000000)
#define A04_NATIVE_OUTPUT_BYTES 4096000u
#define A04_NATIVE_USC_VA UINT64_C(0xba00010000)
#define A04_NATIVE_PDS_VA UINT64_C(0xa800010000)
struct a04_vendor_native_cold {
    struct a04_vendor_render_cold *render;
    struct a04_vendor_uncached data,pt,code[2];
    struct a04_vendor_uncached output[4]; /* pixels, PD, PT0, PT1 */
    struct a04_m111_clear_surface clear_surface;
    struct a04_m111_clear_code_bindings bindings;
    ULONG suffix_offset;
    BOOLEAN output_mapped,backing_staged,quarantined;
    ULONG code_mapped; /* owned USC/PDS PTEs; retained after firmware exposure */
    struct a04_m111_target_storage target;
    struct a04_m111_mlist_storage mlists;
    struct a04_m111_auxiliary_layout auxiliary;
    RGXFWIF_FREELIST freelist[2];
    struct a04_m111_native_addresses addresses;
    uint64_t pm_pages[296]; /* local 40 + global 256, adapter logical addresses */
    ULONG width,height,pm_offset[2],list_offset[2],used;
    BOOLEAN mapped,prepared,bank_prepared,bank_touched;
};

NTSTATUS a04_vendor_native_reserve_code_cold(struct a04_vendor_native_cold *,PDMA_ADAPTER);
NTSTATUS a04_vendor_native_unmap_code_cold(struct a04_vendor_native_cold *);
NTSTATUS a04_vendor_native_validate_allocation_cold(struct a04_vendor_native_cold *,const struct a04_vendor_uncached *);
NTSTATUS a04_vendor_native_prepare_backing_cold(struct a04_vendor_native_cold *,PDMA_ADAPTER,ULONG);
NTSTATUS a04_vendor_native_unmap_output_cold(struct a04_vendor_native_cold *);
#endif
