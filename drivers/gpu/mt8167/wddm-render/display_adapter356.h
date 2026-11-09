/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_DISPLAY_ADAPTER356_H
#define GEO_DISPLAY_ADAPTER356_H
#include "kmd_clear.h"
NTSTATUS GeoDisplay356Start(struct geo_kmd_adapter *,PDEVICE_OBJECT,DXGK_START_INFO *,DXGKRNL_INTERFACE *,ULONG *,ULONG *);
NTSTATUS GeoDisplay356Restore(struct geo_kmd_adapter *); /* actual display no-read, retains GPU backing */
NTSTATUS GeoDisplay356ReleaseMaps(struct geo_kmd_adapter *); /* caller proved owner GPU stop */
void GeoDisplay356EmergencyRestore(struct geo_kmd_adapter *);
NTSTATUS GeoDisplay356Stop(struct geo_kmd_adapter *);
NTSTATUS GeoDisplay356PrimaryLayout(struct geo_kmd_adapter *,const D3DKMDT_SHAREDPRIMARYSURFACEDATA *,struct geo_allocation_data_v2 *);
NTSTATUS GeoDisplay356RetireAllocation(struct geo_kmd_adapter *,struct geo_allocation *);
DXGKDDI_SETVIDPNSOURCEADDRESS GeoDisplay356SetSource;
DXGKDDI_CONTROLINTERRUPT GeoDisplay356ControlInterrupt;
DXGKDDI_GETSCANLINE GeoDisplay356GetScanLine;
DXGKDDI_ESCAPE GeoDisplay356Observe;
void *GeoUnifiedDisplayContext(HANDLE);
NTSTATUS GeoUnifiedQuiesceForPostRelease(HANDLE);
NTSTATUS GeoDisplay356Visibility(HANDLE,const DXGKARG_SETVIDPNSOURCEVISIBILITY *);
NTSTATUS GeoUnifiedDrainNativeForDisplay(struct geo_kmd_adapter *);
#endif
