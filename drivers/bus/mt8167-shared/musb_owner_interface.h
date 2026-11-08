/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_MUSB_OWNER_INTERFACE_H
#define MT8167_MUSB_OWNER_INTERFACE_H
#include <ntddk.h>
/* Our versioned parent/child ABI, NOT a guessed Microsoft callback ABI.
 * All methods PASSIVE, serialized by consumer. Query/Observe are read-only.
 * Neither GPIO source health nor DEVCTL VBUSVALID proves measured 5 V or OC.
 * A producer must have real observations before setting their valid bits. */
static const GUID MT8167MusbOwnerInterfaceGuid =
 {0xf38ff3a6,0x5cd1,0x43a0,{0x97,0x45,0x72,0xcf,0xda,0x16,0x67,0x13}};
#define MT_MUSB_OWNER_VERSION 1
#define MT_MUSB_ROLE 1u
#define MT_MUSB_OC 2u
#define MT_MUSB_VBUS 4u
#define MT_MUSB_EXCLUSIVE 8u
#define MT_MUSB_STOP_CAPABLE 16u
#define MT_MUSB_RESOURCE_ASSIGNED 32u
#define MT_MUSB_REQUIRED 63u
#define MT_MUSB_STOPPED 64u
#define MT_MUSB_IRQ_OFF 128u
#define MT_MUSB_DMA_OFF 256u
#define MT_MUSB_STOP_PROOF (MT_MUSB_STOPPED|MT_MUSB_IRQ_OFF|MT_MUSB_DMA_OFF)
typedef struct {
 ULONG bytes,capabilities,valid,host_role,oc_clear,vbus_on;
 ULONGLONG generation,observed_100ns;
 /* Actual assigned child raw/translated CM memory pairs, not guessed identity
  * translations. Values remain stable for a lease generation. */
 PHYSICAL_ADDRESS raw_mac,mac,raw_phy,phy;ULONG mac_bytes,phy_bytes;
} MT_MUSB_OBSERVATION;
typedef struct {
 INTERFACE Header;
 NTSTATUS (*Query)(PVOID,MT_MUSB_OBSERVATION *); /* no lease / no MMIO edits */
 NTSTATUS (*Acquire)(PVOID,ULONGLONG); /* compare queried generation atomically */
 NTSTATUS (*SetVbus)(PVOID,BOOLEAN);
 NTSTATUS (*Observe)(PVOID,MT_MUSB_OBSERVATION *);
 /* Deterministic parent isolation fallback, after consumer masks/flushes PIO.
  * Failure retains the lease. Never equate a requested stop to observed idle. */
 NTSTATUS (*StopAndProve)(PVOID,MT_MUSB_OBSERVATION *);
 NTSTATUS (*Release)(PVOID);
} MT_MUSB_OWNER_INTERFACE;
#endif
