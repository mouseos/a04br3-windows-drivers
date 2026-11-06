/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GCE_DISPLAY_INTERFACE_H
#define A04_GCE_DISPLAY_INTERFACE_H
#include <ntddk.h>
static const GUID A04GceDeviceInterfaceGuid =
 {0x854b9272,0x4644,0x435d,{0x92,0xf4,0x39,0x60,0xf9,0x38,0xa4,0x16}};
static const GUID A04GceDisplayInterfaceGuid =
 {0x7b8ec894,0xf039,0x44c3,{0x8a,0x7f,0x10,0x4e,0x3d,0x1c,0x60,0xa4}};
#define A04_GCE_DISPLAY_INTERFACE_VERSION 4u
/* Thread 0 snapshot only. No field establishes secure-world inactivity. */
typedef struct _A04_GCE_THREAD_OBSERVATION {
    ULONG enable,status,irq_status,irq_enable;
} A04_GCE_THREAD_OBSERVATION;
typedef struct _A04_GCE_DISPLAY_INTERFACE {
    INTERFACE Header;
    NTSTATUS (*Acquire)(PVOID,ULONG *);
    NTSTATUS (*Release)(PVOID,ULONG);
    NTSTATUS (*Ready)(PVOID,ULONG);
    /* Returns GCE EOC only; not a display latch or DMA-stop proof. */
    NTSTATUS (*Submit)(PVOID,ULONG,ULONG,ULONG,ULONG);
    NTSTATUS (*ObserveThread0)(PVOID,ULONG,A04_GCE_THREAD_OBSERVATION *);
} A04_GCE_DISPLAY_INTERFACE;
#endif
