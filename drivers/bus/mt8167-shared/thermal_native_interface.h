/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_THERMAL_NATIVE_INTERFACE_H
#define A04_SHARED_THERMAL_NATIVE_INTERFACE_H
#include <ntddk.h>
/* V2: synchronous parent-owned PIO, no caller hardware callbacks or IRQ/DMA.
 * Adopted==1 transfers both MmMapIoSpace mappings even on measurement failure.
 * Supplier retains uncertain mappings/clock lease; caller must never free them.
 * Only the actual thermal PDO with the checked exclusive two ranges may bind. */
static const GUID A04SharedThermalNativeGuid={0x481fa133,0x4946,0x43ee,{0xa8,0x14,0x94,0x88,0x70,0xa4,0x10,0x09}};
#define A04_THERMAL_NATIVE_VERSION 2
#define A04_THERMAL_NATIVE_PARENT_BUILD 1018u
typedef struct _A04_THERMAL_NATIVE_INTERFACE {
 INTERFACE Header;ULONG ParentBuild;
 NTSTATUS (*Measure)(PVOID,PVOID,PVOID,ULONG *,ULONG raw[3],ULONG *stage);
} A04_THERMAL_NATIVE_INTERFACE;
#endif
