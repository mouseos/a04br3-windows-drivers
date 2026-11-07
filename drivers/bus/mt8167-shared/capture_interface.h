/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_CAPTURE_INTERFACE_H
#define A04_CAPTURE_INTERFACE_H
#include <ntddk.h>
/* Separate GUID preserves the existing playback v11 size/version contract. */
static const GUID A04CaptureInterfaceGuid={0x62fc8841,0xf650,0x4b4f,{0x97,0x2c,0x91,0x18,0xa0,0x41,0xca,0x20}};
#define A04_CAPTURE_INTERFACE_VERSION 1
typedef struct _A04_CAPTURE_INTERFACE {
 INTERFACE Header;
 /* PASSIVE, single parent lock; -1 no acquisition, -2 retained partial state. */
 int (*AcquireInput)(PVOID,ULONG);
 int (*PrepareMaskedIrq)(PVOID,PVOID,ULONG);
 int (*ReleaseUnexposed)(PVOID);
 /* STATUS_NOT_SUPPORTED means unavailable, never a zero-latency success. */
 NTSTATUS (*QueryLatency)(PVOID,PULONG,PULONG,PULONG);
} A04_CAPTURE_INTERFACE;
#endif
