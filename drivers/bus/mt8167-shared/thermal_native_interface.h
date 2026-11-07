/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_THERMAL_NATIVE_INTERFACE_H
#define A04_SHARED_THERMAL_NATIVE_INTERFACE_H
#include <ntddk.h>
/* Fixed kernel-only Linux-style OS claim, not a firmware-release certificate.
 * Consumer must be the actual thermal PDO with exclusive thermal/ADC pages.
 * Failed clock/MMIO/quiesce keeps the lease and provider power reference. */
static const GUID A04SharedThermalNativeGuid={0x481fa133,0x4946,0x43ee,{0xa8,0x14,0x94,0x88,0x70,0xa4,0x10,0x09}};
#define A04_THERMAL_NATIVE_VERSION 1
#define A04_THERMAL_NATIVE_PARENT_BUILD 1012u
typedef struct _A04_THERMAL_NATIVE_INTERFACE {
 INTERFACE Header;
 ULONG ParentBuild;
 NTSTATUS (*Acquire)(PVOID,ULONG *);
 NTSTATUS (*Clock)(PVOID,ULONG);
 NTSTATUS (*Reset)(PVOID,ULONG);
 BOOLEAN (*Begin)(PVOID,ULONG);
 VOID (*End)(PVOID);
 NTSTATUS (*ReadAnalog)(PVOID,ULONG,ULONG *);
 NTSTATUS (*WriteAnalog)(PVOID,ULONG,ULONG);
 NTSTATUS (*Quiescent)(PVOID,ULONG);
 NTSTATUS (*Release)(PVOID,ULONG);
} A04_THERMAL_NATIVE_INTERFACE;
#endif
