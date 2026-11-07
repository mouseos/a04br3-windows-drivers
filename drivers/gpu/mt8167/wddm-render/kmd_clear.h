/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_KMD_CLEAR_H
#define GEO_KMD_CLEAR_H
#ifndef DXGKDDI_INTERFACE_VERSION
#define DXGKDDI_INTERFACE_VERSION 0x4002
#endif
#include <ntddk.h>
#include <dispmprt.h>
#include "clear_engine.h"
#include "owner_interface.h"
/* These are DDI implementation modules, not a DriverEntry that registers an
 * incomplete WDDM adapter. The full miniport owns synchronization/rundown,
 * querycaps, paging and real DIRQL ISR/DPC routing. */
struct geo_kmd_adapter {
 struct geo_engine engine;
 /* Must use the provider's real interrupt synchronization object. No manual
  * fabricated DIRQL and no sleeping callback at interrupt IRQL. */
 BOOLEAN (*synchronize)(void *owner,PKSYNCHRONIZE_ROUTINE routine,void *argument);
 void *sync_context;
};
struct geo_kmd_context { struct geo_kmd_adapter *adapter; };
DXGKDDI_CREATEALLOCATION GeoCreateAllocation;
DXGKDDI_DESTROYALLOCATION GeoDestroyAllocation;
DXGKDDI_BUILDPAGINGBUFFER GeoBuildPagingBuffer;
DXGKDDI_RENDER GeoRender;
DXGKDDI_PATCH GeoPatch;
DXGKDDI_SUBMITCOMMAND GeoSubmitCommand;
DXGKDDI_QUERYCURRENTFENCE GeoQueryCurrentFence;
#endif
