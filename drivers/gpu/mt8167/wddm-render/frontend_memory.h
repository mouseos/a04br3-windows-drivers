/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_FRONTEND_MEMORY_H
#define GEO_FRONTEND_MEMORY_H
#include <ntddk.h>
struct geo_kmd_adapter;
struct geo_allocation;
/* Provenance is the actual VidMm MAP_APERTURE DDI for this driver's cached
 * system-memory allocation, plus an independent, PFN-checked page lock.
 * No user MDL, common buffer, guessed cache flag or metadata-only pin. */
#define GEO_FRONTEND_MEMORY_VERSION 1u
#define GEO_MDL_VIDMM_CACHED_SHADOW 1u
struct geo_frontend_mdl_lease {
 ULONG size,version,origin,active_bytes,page_bytes;
 PMDL mdl; /* independently locked SAME PFNs; never a copied private output */
 void *context;
 NTSTATUS (*acquire)(void *,PMDL);
 void (*release)(void *,PMDL);
};
struct geo_frontend_memory {
 ULONG size,version;void *context;
 NTSTATUS (*resolve)(void *,PMDL vidmm_mdl,ULONG active_bytes,struct geo_frontend_mdl_lease *);
};
/* PASSIVE. Resolve returns ONE OWNED reference. After a04_native_mdl_bind calls acquire,
 * caller drops the resolve reference with release on both success/failure.
 * Native passes lease.mdl (not the borrowed VidMm metadata) to its SG/cache
 * helper, and holds acquire until cold detach or proven future retirement.
 * Release only drops a reference to the independent lock; it never unlocks
 * the original VidMm MDL or frees VidMm backing. It is NOT a drain proof. */
void GeoInitializeFrontendMemory(struct geo_kmd_adapter *);
NTSTATUS GeoBeginVidMmMap(struct geo_kmd_adapter *,struct geo_allocation *,PMDL,ULONG active_bytes);
/* Select mapped_pages from borrowed source at a PAGE-based MdlOffset.
 * This currently covers the full allocation (roundup(active_bytes)), not
 * disconnected partial-allocation residency. selected_view is an owned
 * partial MDL identity backed by our independently locked full MDL. Pass
 * selected_view to geo_bind/native with offset=0; never apply MdlOffset twice.
 * The returned view is BORROWED by the caller until GeoEndVidMmMap plus final
 * native lease release. Do not free/probe/unlock it or send it to DMA helpers. */
NTSTATUS GeoBeginVidMmMapSpan(struct geo_kmd_adapter *,struct geo_allocation *,PMDL source,
 ULONG mdl_offset_pages,SIZE_T mapped_pages,ULONG active_bytes,PMDL *selected_view);
NTSTATUS GeoConfirmVidMmBind(struct geo_kmd_adapter *,struct geo_allocation *);
NTSTATUS GeoAcquireScanoutMap(struct geo_kmd_adapter *,struct geo_allocation *,struct geo_frontend_mdl_lease *);
void GeoEndVidMmMap(struct geo_kmd_adapter *,struct geo_allocation *);
/* SEH TU MUST use the real Microsoft ARM compiler, not unsafe dereference or
 * a Clang SEH substitute. The rest of the miniport remains Clang-built. */
NTSTATUS GeoPinVidMmPages(PMDL,ULONG page_bytes,PMDL *retained);
void GeoUnpinVidMmPages(PMDL);
/* Real selected PFNs: pinned is independently probe-locked; view is a true
 * IoBuildPartialMdl identity of pinned, not a partial MDL relabeled as full.
 * Neither owns/unlocks the original WDDM MDL. Destroy view before pinned. */
NTSTATUS GeoPinVidMmSpan(PMDL source,ULONG mdl_offset_pages,ULONG page_bytes,
 PMDL *selected_view,PMDL *retained);
void GeoUnpinVidMmSpan(PMDL selected_view,PMDL retained);
#endif
