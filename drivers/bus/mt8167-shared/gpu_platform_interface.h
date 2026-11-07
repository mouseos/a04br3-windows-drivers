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
/* V2 is a different GUID: only registered for the actually assigned SPM
 * profile. Resource presence is not a claim of completed hardware shutdown. */
static const GUID A04SharedGpuDomainGuid=
 {0x21a5ad67,0xfed5,0x413d,{0x99,0x14,0xc9,0x22,0x01,0x14,0x16,0x67}};
#define A04_SHARED_GPU_DOMAIN_VERSION 2
struct a04_gpu_domain_snapshot {
 ULONG size,version;ULONGLONG token;
 ULONG spm_status,spm_status2,control[3],unlock,protect,protect_ack,way;
 ULONG mux0,mux1,slow;
};
typedef struct _A04_SHARED_GPU_DOMAIN_INTERFACE {
 INTERFACE Header;
 NTSTATUS (*Acquire)(PVOID,ULONGLONG *);
 NTSTATUS (*Observe)(PVOID,ULONGLONG,struct a04_gpu_domain_snapshot *);
 NTSTATUS (*Off)(PVOID,ULONGLONG);
 NTSTATUS (*On)(PVOID,ULONGLONG);
 NTSTATUS (*VerifyOff)(PVOID,ULONGLONG);
 NTSTATUS (*ReleaseOff)(PVOID,ULONGLONG);
 NTSTATUS (*ReleaseOn)(PVOID,ULONGLONG);
} A04_SHARED_GPU_DOMAIN_INTERFACE;
#endif
