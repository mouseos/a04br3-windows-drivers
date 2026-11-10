/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_MUSB_OWNER_INTERFACE_H
#define MT8167_MUSB_OWNER_INTERFACE_H
#include <ntddk.h>
/* Our versioned parent/child ABI, NOT a guessed Microsoft callback ABI.
 * All methods PASSIVE, serialized by consumer. Query is read-only; Observe may cut the source on a live hazard.
 * GPIO21 is a digital OC input observation; DEVCTL VBUSVALID is a comparator
 * band, not measured connector 5 V. Unknown protection capability is not asserted.
 * A producer must have real observations before setting their valid bits. */
static const GUID MT8167MusbOwnerInterfaceGuid =
 {0xf38ff3a6,0x5cd1,0x43a0,{0x97,0x45,0x72,0xcf,0xda,0x16,0x67,0x13}};
#define MT_MUSB_OWNER_VERSION 3
#define MT_MUSB_ROLE 1u
#define MT_MUSB_OC 2u
#define MT_MUSB_VBUS 4u
#define MT_MUSB_EXCLUSIVE 8u
#define MT_MUSB_STOP_CAPABLE 16u
#define MT_MUSB_RESOURCE_ASSIGNED 32u
#define MT_MUSB_REQUIRED 63u
#define MT_MUSB_QUERY_READY 1u
#define MT_MUSB_OC_PIN21 2u
#define MT_MUSB_VBUS_COMPARATOR 4u
#define MT_MUSB_SOURCE_OFF 8u
#define MT_MUSB_NO_FORCED_VBUS 16u
#define MT_MUSB_PHY_SENSING_READY 32u
#define MT_MUSB_MAC_HOST 64u /* SESSION + HM, BDEVICE clear; not GPIO ID */
#define MT_MUSB_WAIT_PC 1u
#define MT_MUSB_WAIT_ROLE 2u
#define MT_MUSB_ROLE_DEPARTED 3u
#define MT_MUSB_HARDWARE_FAULT 4u
#define MT_MUSB_STOPPED 64u
#define MT_MUSB_IRQ_OFF 128u
#define MT_MUSB_DMA_OFF 256u
#define MT_MUSB_STOP_PROOF (MT_MUSB_STOPPED|MT_MUSB_IRQ_OFF|MT_MUSB_DMA_OFF)
typedef struct {
 ULONG bytes,capabilities,valid,host_role,oc_clear,vbus_on;
 /* v2 provenance: comparator status is not a 5V measurement. GPIO21 is the
  * board-described active-low OC input; this does not certify a protector. */
 ULONG evidence,reason,devctl,intrusb,gpio21,gpio22,gpio41,gpio34;
 ULONG source_off;
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
