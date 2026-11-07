/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_LIFECYCLE_H
#define GEO_LIFECYCLE_H
#include "kmd_clear.h"
#include "owner_control.h"
#define GEO_SESSION_ACTIVE 1
#define GEO_SESSION_HALTED 2
#define GEO_SESSION_SUSPENDED 3
#define GEO_SESSION_RESET 4
#define GEO_SESSION_FAILED 5
#define GEO_SESSION_STOPPED 6
#define GEO_SESSION_TRANSITION 7
struct geo_owner_session {
 struct geo_kmd_adapter *adapter;
 UINT interface_referenced;
 struct geo_owner_control_v2 control;
 struct geo_owner_lease lease;
 KMUTEX control_lock,passive_lock;
 EX_RUNDOWN_REF operations;
 volatile LONG state;
 UINT initialized,rundown_closed;
};
/* In-process native core inside the SAME render miniport. No second FDO,
 * no QueryInterface and no second interrupt owner. Control is a trusted
 * source-linked table, held by InterfaceReference until proven release.
 * Caller owns zeroed nonpaged session/adapter through successful stop. */
NTSTATUS GeoStartInProcessOwnerSession(struct geo_owner_session *,struct geo_kmd_adapter *,const struct geo_owner_control_v2 *,const DXGKRNL_INTERFACE *);
NTSTATUS GeoStopOwnerSession(struct geo_owner_session *);
NTSTATUS GeoPowerOwnerSession(struct geo_owner_session *,DEVICE_POWER_STATE);
NTSTATUS GeoResetOwnerSession(struct geo_owner_session *);
NTSTATUS GeoRestartOwnerSession(struct geo_owner_session *);
BOOLEAN GeoOwnerInterrupt(struct geo_owner_session *,ULONG message);
/* mode: 0=DISPATCH operation, 1=PASSIVE GPU operation, 2=PASSIVE cleanup.
 * Cleanup also works in proven HALTED D0, so retained allocations can be
 * unmapped/destroyed before a subsequent Stop releases the actual lease. */
BOOLEAN GeoSessionEnter(struct geo_kmd_adapter *,UINT mode);
void GeoSessionLeave(struct geo_kmd_adapter *,UINT mode);
DXGKDDI_RESETFROMTIMEOUT GeoResetFromTimeout;
DXGKDDI_RESTARTFROMTIMEOUT GeoRestartFromTimeout;
#endif
