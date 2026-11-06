/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GCE_CLOCK_INTERFACE_H
#define A04_GCE_CLOCK_INTERFACE_H
#include <ntddk.h>
/* The device interface locates the sibling parent; QUERY_INTERFACE retrieves
 * the separately versioned kernel callbacks. No interface pointer proves D0,
 * firmware release, event ownership or hardware quiescence. */
static const GUID A04GceParentDeviceInterfaceGuid =
 {0x758708d2,0x419a,0x4f85,{0xa0,0x19,0x6b,0x7c,0xb8,0x1e,0x72,0x31}};
static const GUID A04GceClockInterfaceGuid =
 {0x92bd016f,0x8dc2,0x4a1b,{0x90,0xa7,0xb5,0x62,0x51,0x78,0x09,0xf4}};
#define A04_GCE_CLOCK_INTERFACE_VERSION 1u
typedef struct _A04_GCE_CLOCK_INTERFACE {
    INTERFACE Header;
    NTSTATUS (*Acquire)(PVOID,ULONG *);
    NTSTATUS (*Release)(PVOID,ULONG);
} A04_GCE_CLOCK_INTERFACE;
#endif
