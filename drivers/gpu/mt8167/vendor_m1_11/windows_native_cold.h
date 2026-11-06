/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_M111_WINDOWS_NATIVE_COLD_H
#define MT8167_M111_WINDOWS_NATIVE_COLD_H
#include <ntddk.h>
#include <stdint.h>
#define A04_NATIVE_DATA_VA UINT64_C(0x65c0200000)
#define A04_NATIVE_DATA_LIMIT 0x200000u
struct a04_vendor_native_cold;
struct a04_vendor_render_cold;
struct a04_m111_hwrt_input;
/* Opaque boundary: m1.11 DDK types must not enter the modern FWIF provider TU.
 * The allocation is retained once the parent is hardware-owned. */
NTSTATUS a04_vendor_native_allocate_cold(struct a04_vendor_native_cold **,
    struct a04_vendor_render_cold *,PDMA_ADAPTER,ULONG map_registers,
    ULONG width,ULONG height);
NTSTATUS a04_vendor_native_release_cold(struct a04_vendor_native_cold **);
ULONG a04_vendor_native_bytes(const struct a04_vendor_native_cold *);
/* Independently derived HWRT register input; bank bind is not drawing. */
NTSTATUS a04_vendor_native_bind_bank_cold(struct a04_vendor_native_cold *,
    const struct a04_m111_hwrt_input *);
#endif
