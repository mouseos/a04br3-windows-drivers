/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_UNIFIED_DISPLAY_H
#define GEO_UNIFIED_DISPLAY_H
/* Include the WDK display types before this header. No second DriverEntry,
 * PDO, interrupt registration, native GPU context, or QueryInterface owner. */
#ifdef __cplusplus
extern "C" {
#endif
NTSTATUS GeoDisplayCreate(PDEVICE_OBJECT pdo,void **display);
NTSTATUS GeoDisplayStart(void *display,DXGK_START_INFO *,DXGKRNL_INTERFACE *,ULONG *sources,ULONG *children);
NTSTATUS GeoDisplayStop(void *display);
NTSTATUS GeoDisplayDelete(void *display);
NTSTATUS GeoDisplayPostInfo(void *display,DXGK_DISPLAY_INFORMATION *info);
/* Source metadata only: no physical/GPU address is exposed as private data.
 * CreateAllocation must revalidate against this actual POST owner; this does
 * not allocate VidMm storage or claim a native primary binding. */
struct geo_post_primary_layout_v1 {
 UINT size,version,source_id,width,height,pitch,format,reserved;
};
NTSTATUS GeoDisplaySharedPrimaryLayout(void *,const D3DKMDT_SHAREDPRIMARYSURFACEDATA *,struct geo_post_primary_layout_v1 *);
/* Validates only the already scanned-out physical POST surface. It never
 * substitutes an aperture VA or arbitrary allocation for scanout memory. */
NTSTATUS GeoDisplayValidatePostAddress(void *,const DXGKARG_SETVIDPNSOURCEADDRESS *);

/* Implemented by the unified render wrapper, not a second miniport. */
void *GeoUnifiedDisplayContext(HANDLE adapter);
/* Fill real BDD VidPn/POST callbacks only. Does not register an adapter,
 * modify caps, report sources, or supply Present/SetSourceAddress. */
NTSTATUS GeoDisplayFillVidPnCallbacks(DRIVER_INITIALIZATION_DATA *table);
NTSTATUS GeoDisplayPower(void *,ULONG,DEVICE_POWER_STATE,POWER_ACTION);
#ifdef __cplusplus
}
#endif
#endif
