/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_M1_11_WINDOWS_STAGE_H
#define A04_VENDOR_M1_11_WINDOWS_STAGE_H
#include <ntddk.h>
#include "stage.h"
struct a04_vendor_m1_11_memory {
    PDMA_ADAPTER adapter;
    struct a04_vendor_buffer buffers[3];
    PHYSICAL_ADDRESS logical[3];
    void *allocation[3];
    BOOLEAN staged;
    BOOLEAN hardware_owned;
};
/* PASSIVE_LEVEL only. No hardware access or FWIF runtime creation. */
NTSTATUS a04_vendor_m1_11_windows_stage(struct a04_vendor_m1_11_memory *m,
    PDMA_ADAPTER adapter,const void *image,size_t bytes);
/* A hardware-owned allocation must remain pinned until a separately proven
 * DMA-quiescence path exists; this function will not free it. */
NTSTATUS a04_vendor_m1_11_windows_free(struct a04_vendor_m1_11_memory *m);
#endif
