/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_THERMAL_INTERFACE_H
#define A04_SHARED_THERMAL_INTERFACE_H
#include <ntddk.h>
/* Kernel-only, PASSIVE_LEVEL. A reference preserves the parent object, not
 * the physical device. Begin/End protect only one nonblocking MMIO access. */
static const GUID A04SharedThermalInterfaceGuid =
 {0xa07ba8fc,0x89e1,0x4e7a,{0x83,0x21,0x30,0x6a,0x1a,0x08,0xc2,0x74}};
#define A04_SHARED_THERMAL_INTERFACE_VERSION 1
#define A04_SHARED_THERMAL_IFACE_SIGNATURE 0x54413430u
typedef struct _A04_SHARED_THERMAL_INTERFACE {
 INTERFACE Header;
 NTSTATUS (*Acquire)(PVOID,ULONG *);
 NTSTATUS (*Clock)(PVOID,ULONG);
 NTSTATUS (*Reset)(PVOID,ULONG);
 BOOLEAN (*Begin)(PVOID,ULONG);
 VOID (*End)(PVOID);
 NTSTATUS (*Release)(PVOID,ULONG);
} A04_SHARED_THERMAL_INTERFACE;
#endif
