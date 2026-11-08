/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "kmd_objects.h"
NTSTATUS APIENTRY GeoCreateDevice(HANDLE handle,DXGKARG_CREATEDEVICE *p) {
 struct geo_kmd_adapter *a=handle;struct geo_kmd_device *d;NTSTATUS status=STATUS_SUCCESS;
 if(!p||(p->Flags.Value&~1u))return STATUS_NOT_SUPPORTED;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 if(a->device_count==UINT32_MAX){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 d=ExAllocatePoolWithTag(NonPagedPool,sizeof(*d),'dOeG');if(!d){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 RtlZeroMemory(d,sizeof(*d));d->adapter=a;d->runtime=p->hDevice;d->next=a->devices;a->devices=d;a->device_count++;p->hDevice=d;
 done:GeoSessionLeave(a,1);return status;
}
NTSTATUS APIENTRY GeoDestroyDevice(HANDLE handle) {
 struct geo_kmd_device *d=handle,**link;struct geo_kmd_adapter *a;NTSTATUS status=STATUS_SUCCESS;
 if(!d||!(a=d->adapter)||!GeoSessionEnter(a,2))return STATUS_DEVICE_NOT_READY;
 if(d->contexts||d->opens){status=STATUS_DEVICE_BUSY;goto done;}
 for(link=&a->devices;*link&&*link!=d;link=&(*link)->next){}
 if(!*link||!a->device_count){status=STATUS_INVALID_PARAMETER;goto done;}
 *link=d->next;a->device_count--;ExFreePoolWithTag(d,'dOeG');
 done:GeoSessionLeave(a,2);return status;
}
NTSTATUS APIENTRY GeoCreateContext(HANDLE handle,DXGKARG_CREATECONTEXT *p) {
 struct geo_kmd_device *d=handle;struct geo_kmd_adapter *a;struct geo_kmd_context *c;NTSTATUS status=STATUS_SUCCESS;
 if(!d||!p||p->NodeOrdinal||p->EngineAffinity!=1||(p->Flags.Value&~1u)||p->PrivateDriverDataSize)return STATUS_NOT_SUPPORTED;
 a=d->adapter;if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 c=ExAllocatePoolWithTag(NonPagedPool,sizeof(*c),'xOeG');if(!c){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 RtlZeroMemory(c,sizeof(*c));c->adapter=a;c->device=d;c->runtime=p->hContext;c->next=d->contexts;d->contexts=c;
 RtlZeroMemory(&p->ContextInfo,sizeof(p->ContextInfo));p->ContextInfo.DmaBufferSize=GEO_DRAW_TEXTURE_COMMAND_MAX>GEO_DRAW_LINK_COMMAND_MAX?GEO_DRAW_TEXTURE_COMMAND_MAX:GEO_DRAW_LINK_COMMAND_MAX;
 p->ContextInfo.DmaBufferPrivateDataSize=sizeof(struct geo_dma_ticket);/* RT8.1 VidMm AddBuffer raises nonzero allocation-list capacities <4 to
  * 3, then AcquireBuffer compares that actual capacity with this request.
  * Request the stable minimum; Render/Patch still use only named entries. */
 p->ContextInfo.AllocationListSize=3;p->ContextInfo.PatchLocationListSize=3;p->hContext=c;
 done:GeoSessionLeave(a,1);return status;
}
struct context_check {struct geo_kmd_context *context;NTSTATUS status;};
static BOOLEAN context_idle(void *arg) {
 struct context_check *q=arg;UINT i;q->status=STATUS_SUCCESS;
 for(i=0;i<GEO_MAX_JOBS;i++)if(q->context->adapter->engine.jobs[i].state&&q->context->adapter->engine.jobs[i].context==q->context){q->status=STATUS_DEVICE_BUSY;break;}return TRUE;
}
NTSTATUS APIENTRY GeoDestroyContext(HANDLE handle) {
 struct geo_kmd_context *c=handle,**link;struct geo_kmd_adapter *a;struct context_check check;NTSTATUS status;
 if(!c||!(a=c->adapter)||!GeoSessionEnter(a,2))return STATUS_DEVICE_NOT_READY;
 check.context=c;check.status=STATUS_UNSUCCESSFUL;
 if(!a->synchronize(a->sync_context,context_idle,&check)){status=STATUS_DEVICE_NOT_READY;goto done;}
 status=check.status;if(status!=STATUS_SUCCESS)goto done;
 for(link=&c->device->contexts;*link&&*link!=c;link=&(*link)->next){}
 if(!*link){status=STATUS_INVALID_PARAMETER;goto done;}
 *link=c->next;ExFreePoolWithTag(c,'xOeG');
 done:GeoSessionLeave(a,2);return status;
}
NTSTATUS APIENTRY GeoOpenAllocation(HANDLE handle,const DXGKARG_OPENALLOCATION *p) {
 struct geo_kmd_device *d=handle;struct geo_kmd_adapter *a;struct geo_allocation *allocation,*scan;
 struct geo_open_allocation *open;DXGKARGCB_GETHANDLEDATA query;NTSTATUS status=STATUS_SUCCESS;
 if(!d||!p||p->NumAllocations!=1||!p->pOpenAllocation||p->PrivateDriverSize||p->SubresourceIndex||(p->Flags.Value&~3u))return STATUS_NOT_SUPPORTED;
 a=d->adapter;if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&query,sizeof(query));query.hObject=p->pOpenAllocation[0].hAllocation;query.Type=DXGK_HANDLE_ALLOCATION;
 allocation=a->get_handle_data(&query); /* Real dxgkrnl handle translation; never cast a user D3DKMT handle. */
 for(scan=a->engine.allocations;scan&&scan!=allocation;scan=scan->next){}
 if(!scan||allocation->open_count==UINT32_MAX){status=STATUS_INVALID_PARAMETER;goto done;}
 open=ExAllocatePoolWithTag(NonPagedPool,sizeof(*open),'oOeG');if(!open){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 RtlZeroMemory(open,sizeof(*open));open->allocation=allocation;open->readonly=p->Flags.ReadOnly;
 open->next=d->opens;d->opens=open;allocation->open_count++;p->pOpenAllocation[0].hDeviceSpecificAllocation=open;
 done:GeoSessionLeave(a,1);return status;
}
NTSTATUS APIENTRY GeoCloseAllocation(HANDLE handle,const DXGKARG_CLOSEALLOCATION *p) {
 struct geo_kmd_device *d=handle;struct geo_kmd_adapter *a;struct geo_open_allocation **link,*open;NTSTATUS status=STATUS_SUCCESS;
 if(!d||!p||p->NumAllocations!=1||!p->pOpenHandleList)return STATUS_INVALID_PARAMETER;
 a=d->adapter;if(!GeoSessionEnter(a,2))return STATUS_DEVICE_NOT_READY;
 for(link=&d->opens;*link&&*link!=p->pOpenHandleList[0];link=&(*link)->next){}
 if(!*link||!(*link)->allocation->open_count){status=STATUS_INVALID_PARAMETER;goto done;}
 open=*link;*link=open->next;open->allocation->open_count--;ExFreePoolWithTag(open,'oOeG');
 done:GeoSessionLeave(a,2);return status;
}
struct geo_allocation *GeoResolveReadAllocation(struct geo_kmd_context *c,HANDLE handle) {
 struct geo_open_allocation *open;
 if(!c||!c->device)return NULL;
 for(open=c->device->opens;open;open=open->next)if(open==handle)return open->allocation;
 return NULL;
}
struct geo_allocation *GeoResolveWriteAllocation(struct geo_kmd_context *c,HANDLE handle) {
 struct geo_open_allocation *open;
 if(!c||!c->device)return NULL;
 for(open=c->device->opens;open;open=open->next)if(open==handle)return open->readonly?NULL:open->allocation;
 return NULL;
}
NTSTATUS APIENTRY GeoDescribeAllocation(HANDLE handle,DXGKARG_DESCRIBEALLOCATION *p) {
 struct geo_kmd_adapter *a=handle;struct geo_allocation *allocation;NTSTATUS status=STATUS_SUCCESS;
 if(!p)return STATUS_INVALID_PARAMETER;
 if(!GeoSessionEnter(a,2))return STATUS_DEVICE_NOT_READY;
 for(allocation=a->engine.allocations;allocation&&allocation!=p->hAllocation;allocation=allocation->next){}
 if(!allocation){status=STATUS_INVALID_PARAMETER;goto done;}
 p->Width=allocation->logical_width;p->Height=allocation->logical_height;p->Format=(D3DDDIFORMAT)allocation->desc.format;
 p->MultisampleMethod.NumSamples=1;p->MultisampleMethod.NumQualityLevels=1;
 p->RefreshRate.Numerator=0;p->RefreshRate.Denominator=1;p->PrivateDriverFormatAttribute=0;
 p->Flags.Value=0;p->Rotation=D3DDDI_ROTATION_IDENTITY;
 done:GeoSessionLeave(a,2);return status;
}
NTSTATUS APIENTRY GeoGetStandardAllocationDriverData(HANDLE handle,DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA *p) {
 struct geo_kmd_adapter *a=handle;struct geo_surface_desc d;struct geo_allocation_data data;NTSTATUS status=STATUS_SUCCESS;
 if(!p)return STATUS_INVALID_PARAMETER;
 /* d3dkmdt.h: standard staging is implicitly X8R8G8B8 and lockable.
  * This describes CPU-only storage, not GPU render-format support. */
 RtlZeroMemory(&d,sizeof(d));d.size=sizeof(d);d.version=GEO_ABI_VERSION;d.usage=GEO_USAGE_STAGING;
 if(p->StandardAllocationType==D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE) {
  if(!p->pCreateShadowSurfaceData||p->pCreateShadowSurfaceData->Format!=D3DDDIFMT_A8R8G8B8)return STATUS_NOT_SUPPORTED;
  d.width=p->pCreateShadowSurfaceData->Width;d.height=p->pCreateShadowSurfaceData->Height;d.format=GEO_FORMAT_A8R8G8B8;
 } else if(p->StandardAllocationType==D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE) {
  if(!p->pCreateStagingSurfaceData)return STATUS_INVALID_PARAMETER;
  d.width=p->pCreateStagingSurfaceData->Width;d.height=p->pCreateStagingSurfaceData->Height;d.format=GEO_FORMAT_X8R8G8B8;
 } else return STATUS_NOT_SUPPORTED;
 /* Validate the multiplication before publishing Pitch or private data. */
 if(!d.width||!d.height||d.width>1024||d.height>1024)return STATUS_NOT_SUPPORTED;
 d.pitch=d.width*4u;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 if(!geo_desc_valid(&d)){status=STATUS_NOT_SUPPORTED;goto done;}
 RtlZeroMemory(&data,sizeof(data));data.size=sizeof(data);data.version=1;data.logical_width=d.width;data.logical_height=d.height;data.storage=d;
 p->AllocationPrivateDriverDataSize=sizeof(data);p->ResourcePrivateDriverDataSize=0;
 if(p->StandardAllocationType==D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE)p->pCreateStagingSurfaceData->Pitch=d.pitch;
 else p->pCreateShadowSurfaceData->Pitch=d.pitch;
 if(p->pAllocationPrivateDriverData) {
  RtlCopyMemory(p->pAllocationPrivateDriverData,&data,sizeof(data));
 }
 done:GeoSessionLeave(a,1);return status;
}
struct geo_debug_snapshot {UINT size,version;uint64_t epoch;UINT poisoned,allocations,devices,last_submitted,last_completed,prepared,submitted,pinned;};
struct geo_debug_call {struct geo_kmd_adapter *adapter;struct geo_debug_snapshot snapshot;};
static BOOLEAN debug_snapshot(void *arg) {
 struct geo_debug_call *c=arg;struct geo_engine *e=&c->adapter->engine;UINT i;
 c->snapshot.size=sizeof(c->snapshot);c->snapshot.version=1;c->snapshot.epoch=c->adapter->session->lease.epoch;
 c->snapshot.poisoned=e->poisoned;c->snapshot.allocations=e->allocation_count;c->snapshot.devices=c->adapter->device_count;
 c->snapshot.last_submitted=e->last_submitted;c->snapshot.last_completed=e->last_completed;
 for(i=0;i<GEO_MAX_JOBS;i++) {
  if(e->jobs[i].state==1)c->snapshot.prepared++;
  if(e->jobs[i].state==2)c->snapshot.submitted++;
  if(e->jobs[i].state==3||e->jobs[i].state==4)c->snapshot.pinned++;
 }
 return TRUE;
}
NTSTATUS APIENTRY GeoCollectDbgInfo(HANDLE handle,const DXGKARG_COLLECTDBGINFO *p) {
 struct geo_kmd_adapter *a=handle;struct geo_debug_call call;
 if(!a||!a->session||!a->synchronize||!p||!p->pBuffer||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(p->BufferSize<sizeof(call.snapshot))return STATUS_BUFFER_TOO_SMALL;
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;
 if(!a->synchronize(a->sync_context,debug_snapshot,&call))return STATUS_DEVICE_NOT_READY;
 /* Kernel-owned bugcheck diagnostic output. No addresses, raw registers,
  * invented hardware state, or successful fence for aborted work. */
 RtlCopyMemory(p->pBuffer,&call.snapshot,sizeof(call.snapshot));return STATUS_SUCCESS;
}
