/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_GPU_INTERFACE_H
#define A04_SHARED_GPU_INTERFACE_H
#include <ntddk.h>
/* A logical observation lease only. It is not a platform power or DMA lease.
 * All methods execute at PASSIVE_LEVEL. A missing exclusive owner must deny DMA. */
static const GUID A04SharedGpuInterfaceGuid =
 {0xb85835e1,0x7c27,0x4e0b,{0xa7,0xc7,0x54,0xb8,0xf6,0xa0,0x01,0x34}};
#define A04_SHARED_GPU_INTERFACE_VERSION 1
struct a04_gpu_bus_snapshot { ULONG axi_si1,protect_enable,protect_status; };
typedef struct _A04_SHARED_GPU_INTERFACE {
 INTERFACE Header;
 NTSTATUS (*AcquireObservation)(PVOID,ULONG *);
 NTSTATUS (*Snapshot)(PVOID,ULONG,struct a04_gpu_bus_snapshot *);
 NTSTATUS (*AuthorizeDma)(PVOID,ULONG);
 NTSTATUS (*ReleaseObservation)(PVOID,ULONG);
} A04_SHARED_GPU_INTERFACE;
#endif
