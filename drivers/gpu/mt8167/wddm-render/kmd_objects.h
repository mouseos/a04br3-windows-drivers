/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_KMD_OBJECTS_H
#define GEO_KMD_OBJECTS_H
#include "lifecycle.h"
struct geo_open_allocation {struct geo_open_allocation *next;struct geo_allocation *allocation;UINT readonly;};
struct geo_kmd_device {struct geo_kmd_device *next;struct geo_kmd_adapter *adapter;HANDLE runtime;struct geo_open_allocation *opens;struct geo_kmd_context *contexts;};
DXGKDDI_CREATEDEVICE GeoCreateDevice;
DXGKDDI_DESTROYDEVICE GeoDestroyDevice;
DXGKDDI_CREATECONTEXT GeoCreateContext;
DXGKDDI_DESTROYCONTEXT GeoDestroyContext;
DXGKDDI_OPENALLOCATIONINFO GeoOpenAllocation;
DXGKDDI_CLOSEALLOCATION GeoCloseAllocation;
DXGKDDI_DESCRIBEALLOCATION GeoDescribeAllocation;
DXGKDDI_GETSTANDARDALLOCATIONDRIVERDATA GeoGetStandardAllocationDriverData;
DXGKDDI_COLLECTDBGINFO GeoCollectDbgInfo;
/* Called only while the adapter PASSIVE lock/rundown is held. */
struct geo_allocation *GeoResolveReadAllocation(struct geo_kmd_context *,HANDLE);
struct geo_allocation *GeoResolveWriteAllocation(struct geo_kmd_context *,HANDLE);
#endif
