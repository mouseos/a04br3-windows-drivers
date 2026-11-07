/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_OWNER_CONTROL_H
#define GEO_OWNER_CONTROL_H
#include <ntddk.h>
#include "owner_abi.h"
/* Proposed v2 control interface. Operations require GEO_OWNER_OPS_VERSION=2; old bind signatures are refused.
 * QueryInterface returns ONE referenced interface. No user-mode transport. */
DEFINE_GUID(GUID_GEO_RENDER_OWNER_CONTROL_V2,0x36871ace,0x58aa,0x49b8,0xb6,0xf3,0x2e,0x51,0xd5,0xe7,0xf8,0x93);
#define GEO_OWNER_CONTROL_VERSION 2u
#define GEO_IRQ_DXGK_REGISTERED 1u
#define GEO_QUIESCE_STOP 1u
#define GEO_QUIESCE_POWER 2u
#define GEO_QUIESCE_TDR 3u
#define GEO_PROOF_DMA_STOPPED 1u
#define GEO_PROOF_IRQ_DRAINED 2u
#define GEO_PROOF_TOKENS_RETIRED 4u
#define GEO_PROOF_BINDINGS_RETAINED 8u
#define GEO_PROOF_CPU_VISIBLE 16u
#define GEO_PROOF_REQUIRED 31u
struct geo_owner_proof { uint32_t size,version;uint64_t epoch;uint32_t flags,reserved; };
struct geo_owner_lease {
 uint32_t size,version;void *cookie;uint64_t epoch;
 uint32_t irq_route,irq_level;struct geo_owner_ops ops;
};
struct geo_owner_irq_event {
 uint32_t size,version;uint64_t epoch,job;
 uint32_t generation;int32_t status;uint32_t visibility,reserved;
};
struct geo_owner_control_v2 {
 INTERFACE Interface;
 /* PASSIVE acquire: real D0/render ownership, IRQ initially masked until
  * restart after notification initialization. Never break an active DOD
  * lease. Output cookie/epoch/ops are valid until a proven release succeeds. */
 NTSTATUS (*acquire)(void *provider,struct geo_owner_lease *out);
 /* PASSIVE: return success only after real DMA stop + interrupt callback
  * rundown + retirement of every prepared/submitted job. Keep surfaces and
  * exact MDL/PFN bindings intact. IRQ masking alone does not satisfy this. */
 NTSTATUS (*quiesce)(void *cookie,uint64_t expected_epoch,uint32_t reason,struct geo_owner_proof *out);
 /* PASSIVE reset must perform real reset/drain. Same retained mappings,
  * strictly newer epoch, no delayed completion from the retired epoch. */
 NTSTATUS (*reset)(void *cookie,uint64_t expected_epoch,struct geo_owner_proof *out);
 NTSTATUS (*restart)(void *cookie,uint64_t epoch);
 NTSTATUS (*set_power)(void *cookie,uint64_t epoch,DEVICE_POWER_STATE state);
 /* PASSIVE release refuses live surfaces/bindings/jobs/callbacks. On error
  * all interface/lease/page lifetimes remain owned and must be retained. */
 NTSTATUS (*release)(void *cookie,uint64_t epoch);
 /* Actual registered Dxgk ISR + owner IRQ lock only. One verified event.
  * STATUS_NOT_FOUND = no completion to consume, NOT a completed job.
  * It must not wait for FW, perform pageable cache work, or invent CPU_VISIBLE.
  * If completion needs PASSIVE service the owner needs an actual hardware
  * completion interrupt after that work, not a manually raised IRQL. */
 /* STATUS_PENDING: real source claimed/masked, but no completed DMA.
  * Return initialized event with current epoch, job=visibility=0. Caller
  * queues its actual DPC/PASSIVE service, never a completion notification. */
 NTSTATUS (*interrupt_event)(void *cookie,ULONG message,struct geo_owner_irq_event *out);
 /* PASSIVE after a claimed source. Real FWCCB/cache/rearm work only;
  * success is service completion, NOT GPU_DONE or CPU_VISIBLE delivery. */
 NTSTATUS (*service)(void *cookie,uint64_t epoch);
 BOOLEAN (*synchronize)(void *cookie,PKSYNCHRONIZE_ROUTINE,void *argument);
};
/* acquire irq_route must be GEO_IRQ_DXGK_REGISTERED: owner has explicitly
 * handed off/exposed the actual interrupt to this miniport's registered ISR.
 * Existing KMDF ISR ownership cannot silently be duplicated by a child PDO.
 * Provider PnP/interrupt topology is a required integration decision. */
#endif
