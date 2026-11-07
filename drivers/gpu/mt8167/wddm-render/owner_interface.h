/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_OWNER_INTERFACE_H
#define GEO_OWNER_INTERFACE_H
#include <ntddk.h>
#include "owner_abi.h"
/* DRAFT query-interface identifier, not implemented/advertised by the current
 * provider. No IOCTL, user pointers, raw addresses or arbitrary GPU commands.
 * Agree this interface with the GPU owner before publishing the GUID. */
DEFINE_GUID(GUID_GEO_RENDER_OWNER_DRAFT_V1,0x36871ace,0x58aa,0x49b8,0xb6,0xf3,0x2e,0x51,0xd5,0xe7,0xf8,0x92);
typedef void (*GEO_OWNER_COMPLETION)(void *consumer,uint64_t job,uint32_t generation,int32_t status,uint32_t visibility);
struct geo_owner_interface_v1 {
 INTERFACE Interface;
 struct geo_owner_ops ops;
 /* PASSIVE registration under referenced D0 lease. Completion delivery must
  * be part of the render miniport's real DxgkDdiInterruptRoutine routing,
  * not an unrelated passive poll or synthetic raised-IRQL callback.
  * unregister succeeds only after callback rundown and real device drain;
  * on failure retain interface, allocations and consumer context. */
 NTSTATUS (*register_consumer)(void *,GEO_OWNER_COMPLETION,void *consumer);
 NTSTATUS (*unregister_consumer)(void *,void *consumer);
 /* Actual interrupt lock; supplied routine must be resident and nonwaiting. */
 BOOLEAN (*synchronize)(void *,PKSYNCHRONIZE_ROUTINE,void *argument);
};
/* InterfaceReference/Dereference are mandatory. Version=1 and exact Size.
 * PnP query transport, D0 lifetime and adapter start/stop remain integration
 * responsibilities. Query success alone never establishes GPU ownership. */
#endif
