/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_HEADPHONE_INTERFACE_H
#define A04_HEADPHONE_INTERFACE_H
#include <ntddk.h>
#include "../../input/mt8167-accdet/socket.h"
/* Independent extension of the same parent; audio v11 and thermal V3 stay
 * byte-compatible. No caller mappings, callbacks, physical addresses or
 * supplied coefficients cross this interface. */
static const GUID A04HeadphoneParentGuid={0xd39057d6,0x722d,0x4c82,{0x9a,0xc4,0x6a,0x94,0xe2,0x98,0x75,0x28}};
#define A04_HP_PARENT_VERSION 1
#define A04_HP_RAW_BYTES 72
#define A04_HP_PHASE_UNKNOWN 0u
#define A04_HP_PHASE_A 1u
#define A04_HP_PHASE_B 2u
#define A04_HP_PHASE_C 3u
#define A04_HP_READY_MEASURED 1u
#define A04_HP_READY_SENSE 2u
#define A04_HP_READY_ANALOG 4u
#define A04_HP_READY_LATENCY 8u
#define A04_HP_DRAIN_UNKNOWN 0u
#define A04_HP_DRAIN_RETAINED 1u
#define A04_HP_DRAIN_RELEASED 2u
#include "headphone_observation.h"
typedef struct _A04_HEADPHONE_INTERFACE {
 INTERFACE Header;ULONG ParentBuild;
 NTSTATUS (*BeginAdc)(PVOID,ULONG *generation);
 NTSTATUS (*ReadAdc)(PVOID,ULONG generation,ULONG channel,struct a04_hp_raw_sample *);
 NTSTATUS (*EndAdc)(PVOID,ULONG generation);
 NTSTATUS (*Readiness)(PVOID,ULONG *flags);
 NTSTATUS (*ReadJack)(PVOID,struct mt8167_accdet_socket *);
 NTSTATUS (*Start)(PVOID);
 NTSTATUS (*MuteRetained)(PVOID);
 NTSTATUS (*ResumeRetained)(PVOID);
 NTSTATUS (*Stop)(PVOID,ULONG *drain_state);
 NTSTATUS (*Latency)(PVOID,ULONG *,ULONG *,ULONG *);
} A04_HEADPHONE_INTERFACE;
#endif
