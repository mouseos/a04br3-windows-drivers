/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GPU_PLATFORM_INTERFACE_H
#define A04_GPU_PLATFORM_INTERFACE_H
#include <ntddk.h>
/* Kernel QUERY_INTERFACE on the legitimate shared parent. Discovery is a
 * device interface, not a GPU command IOCTL, second GPU PDO or IRQ owner.
 * PASSIVE only. Acquire pins real parent D0; release cannot discard isolation,
 * a poisoned lease or ambiguous hardware. Consumer also holds INTERFACE ref.
 * ecaa1008 resources support INFRA protection only. No DMA-stop/cold-reset
 * proof or SPM operation is exposed by this v1 interface. */
static const GUID A04SharedGpuPlatformGuid=
 {0xf7f64c95,0x80a3,0x42be,{0x8b,0xcd,0x09,0xf2,0xa0,0x14,0x16,0x67}};
#define A04_SHARED_GPU_PLATFORM_VERSION 1
#define A04_SHARED_GPU_PLATFORM_INFRA_ONLY 1u
typedef struct _A04_SHARED_GPU_PLATFORM_INTERFACE {
 INTERFACE Header;
 ULONG Profile;
 NTSTATUS (*Acquire)(PVOID,ULONGLONG *);
 NTSTATUS (*Protect)(PVOID,ULONGLONG);
 NTSTATUS (*Unprotect)(PVOID,ULONGLONG);
 NTSTATUS (*Release)(PVOID,ULONGLONG);
} A04_SHARED_GPU_PLATFORM_INTERFACE;
#endif
