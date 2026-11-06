/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_M111_WINDOWS_NATIVE_COLD_PRIVATE_H
#define MT8167_M111_WINDOWS_NATIVE_COLD_PRIVATE_H
#include "windows_native_cold.h"
#include "windows_render_cold.h"
#include "native_resources.h"
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

#endif
