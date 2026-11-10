/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_CLOCK_SNAPSHOT_INTERFACE_H
#define MT8167_CLOCK_SNAPSHOT_INTERFACE_H
#include <ntddk.h>
#include "clock_snapshot.h"
/* Kernel-only. Use WdfFdoQueryForInterface on an existing child FDO.
 * Query obtains a parent object reference; call InterfaceDereference once.
 * This reference does not make hardware present or pin a stale D0 mapping. */
static const GUID Mt8167ClockSnapshotInterfaceGuid =
 {0x9f1f506c,0x3a44,0x4d96,{0x98,0xe7,0x24,0xc0,0xc4,0xe2,0x91,0x67}};
#define MT8167_CLOCK_INTERFACE_VERSION 1u
#define MT8167_CLOCK_PARENT_RESOURCE_CONTRACT 1117u
typedef struct _MT8167_CLOCK_SNAPSHOT_INTERFACE {
    INTERFACE Header;
    ULONG ParentResourceContract;
    /* Exact buffer size, PASSIVE_LEVEL, nonpaged kernel output.
     * STATUS_SUCCESS means two raw sweeps completed, not stable/effective Hz.
     * On I/O failure a partial snapshot may be returned; check status/masks. */
    NTSTATUS (*Snapshot)(PVOID,ULONG,struct mt8167_clock_snapshot *);
} MT8167_CLOCK_SNAPSHOT_INTERFACE;
#endif
