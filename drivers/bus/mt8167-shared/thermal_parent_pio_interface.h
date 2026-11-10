/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_THERMAL_PARENT_PIO_INTERFACE_H
#define MT8167_THERMAL_PARENT_PIO_INTERFACE_H
#include <ntddk.h>
/* V3 has no caller pointers or mapping transfer. Only the parent owns the
 * two translated ranges from its eight-range ACPI assignment. Resource-less
 * thermal child consumes temperatures; old V2 child must be updated together. */
static const GUID Mt8167ThermalParentPioGuid={0x176ef135,0x78da,0x48bf,{0xab,0x65,0x61,0x45,0xb7,0x38,0x89,0xda}};
#define MT8167_THERMAL_PARENT_PIO_VERSION 3
#define MT8167_THERMAL_PARENT_PIO_BUILD 1104u
typedef struct _MT8167_THERMAL_PARENT_PIO_INTERFACE {
 INTERFACE Header;ULONG ParentBuild;
 NTSTATUS (*Measure)(PVOID,ULONG *owner_active,ULONG raw[3],ULONG *stage);
} MT8167_THERMAL_PARENT_PIO_INTERFACE;
#endif
