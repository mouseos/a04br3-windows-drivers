/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_PARENT_CLOCK_CLIENT_H
#define MT8167_PARENT_CLOCK_CLIENT_H
#include <ntddk.h>
#include <wdf.h>
#include "../../bus/mt8167-shared/clock_snapshot_interface.h"
/* Call from an existing child's PASSIVE work item; no standalone HW driver. */
NTSTATUS Mt8167ReadParentClockSnapshot(WDFDEVICE,
                                      struct mt8167_clock_snapshot *);
#endif
