/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "kmd_objects.h"
#include "allocation_batch.h"
#include "display_adapter356.h"
#include "../vendor_m1_11/native_factory_checkpoint.h"
static NTSTATUS APIENTRY create_device365(HANDLE handle,DXGKARG_CREATEDEVICE *p) {
 struct geo_kmd_adapter *a=handle;struct geo_kmd_device *d;NTSTATUS status=STATUS_SUCCESS;
 /* WDK8.1 SystemDevice and GdiDevice share tracked device ownership.
  * GDI is software shadow + Present; this does not advertise RenderKm. */
 if(!p||(p->Flags.Value&~3u))return STATUS_NOT_SUPPORTED;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 if(a->device_count==UINT32_MAX){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 d=ExAllocatePoolWithTag(NonPagedPool,sizeof(*d),'dOeG');if(!d){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 RtlZeroMemory(d,sizeof(*d));d->adapter=a;d->runtime=p->hDevice;d->next=a->devices;a->devices=d;a->device_count++;p->hDevice=d;
 done:GeoSessionLeave(a,1);return status;
}
NTSTATUS APIENTRY GeoCreateDevice(HANDLE handle,DXGKARG_CREATEDEVICE *p) {
 struct geo_kmd_adapter *a=handle;NTSTATUS status;ULONG flags=p?p->Flags.Value:0xffffffffu;
 GeoFactoryCheckpoint(82,STATUS_PENDING,flags,a&&a->session?(ULONG)a->session->state:0xffffffffu,0,0);
 status=create_device365(handle,p);
 GeoFactoryCheckpoint(83,status,flags,a&&a->session?(ULONG)a->session->state:0xffffffffu,0,0);
 return status;
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
static NTSTATUS APIENTRY create_context365(HANDLE handle,DXGKARG_CREATECONTEXT *p) {
 struct geo_kmd_device *d=handle;struct geo_kmd_adapter *a;struct geo_kmd_context *c;NTSTATUS status=STATUS_SUCCESS;
 if(!d||!p||p->NodeOrdinal||p->EngineAffinity!=1||(p->Flags.Value&~3u)||p->PrivateDriverDataSize)return STATUS_NOT_SUPPORTED;
 a=d->adapter;if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 c=ExAllocatePoolWithTag(NonPagedPool,sizeof(*c),'xOeG');if(!c){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 RtlZeroMemory(c,sizeof(*c));c->adapter=a;c->device=d;c->runtime=p->hContext;c->next=d->contexts;d->contexts=c;
 RtlZeroMemory(&p->ContextInfo,sizeof(p->ContextInfo));p->ContextInfo.DmaBufferSize=GEO_DRAW_MATRIX_TEXTURE_COMMAND_MAX>GEO_DRAW_LINK_COMMAND_MAX?GEO_DRAW_MATRIX_TEXTURE_COMMAND_MAX:GEO_DRAW_LINK_COMMAND_MAX;
 p->ContextInfo.DmaBufferPrivateDataSize=sizeof(struct geo_dma_ticket);/* RT8.1 VidMm AddBuffer raises nonzero allocation-list capacities <4 to
  * 3, then AcquireBuffer compares that actual capacity with this request.
  * Request the stable minimum; Render/Patch still use only named entries. */
 /* GDI has a fixed WDK allocation-list contract independent of the
  * smaller list requested by the retained ordinary UMD context. */
 p->ContextInfo.AllocationListSize=p->Flags.GdiContext?DXGK_ALLOCATION_LIST_SIZE_GDICONTEXT:3;
 p->ContextInfo.PatchLocationListSize=3;p->hContext=c;
 done:GeoSessionLeave(a,1);return status;
}
NTSTATUS APIENTRY GeoCreateContext(HANDLE handle,DXGKARG_CREATECONTEXT *p) {
 NTSTATUS status;ULONG flags=p?p->Flags.Value:0xffffffffu,node=p?p->NodeOrdinal:0xffffffffu;
 ULONG affinity=p?p->EngineAffinity:0xffffffffu,bytes=p?p->PrivateDriverDataSize:0xffffffffu;
 GeoFactoryCheckpoint(84,STATUS_PENDING,flags,node,affinity,bytes);
 status=create_context365(handle,p);
 GeoFactoryCheckpoint(85,status,flags,node,affinity,bytes);return status;
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
struct allocation_open_batch {
 struct geo_kmd_adapter *a;UINT count;struct geo_open_allocation *opens[GEO_ALLOCATION_BATCH_MAX];
 struct geo_allocation *allocations[GEO_ALLOCATION_BATCH_MAX];NTSTATUS status;
};
static BOOLEAN allocation_open_commit(void *arg)
{
 struct allocation_open_batch *b=arg;UINT i;
 b->status=STATUS_SUCCESS;
 if(b->a->engine.poisoned){b->status=GEO_POISONED;return TRUE;}
 for(i=0;i<b->count;i++){
  struct geo_allocation *x=b->opens[i]->allocation;
  if(x->poisoned){b->status=GEO_POISONED;return TRUE;}
  if(x->open_count==UINT32_MAX){b->status=STATUS_INTEGER_OVERFLOW;return TRUE;}
 }
 /* Distinct translated allocations, fully allocated open objects, and no
  * fallible work in commit. Same real IRQ lock as close/submit/completion. */
 for(i=0;i<b->count;i++)b->opens[i]->allocation->open_count++;
 return TRUE;
}
NTSTATUS APIENTRY GeoOpenAllocation(HANDLE handle,const DXGKARG_OPENALLOCATION *p) {
 struct geo_kmd_device *d=handle;struct geo_kmd_adapter *a;struct geo_allocation *allocation,*scan;
 struct allocation_open_batch *b;DXGKARGCB_GETHANDLEDATA query;UINT i,j;NTSTATUS status;
 if(!d||!p||!p->NumAllocations||p->NumAllocations>GEO_ALLOCATION_BATCH_MAX||
    !p->pOpenAllocation||p->SubresourceIndex||(p->Flags.Value&~3u))return STATUS_NOT_SUPPORTED;
 a=d->adapter;if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 b=ExAllocatePoolWithTag(NonPagedPool,sizeof(*b),'bAeG');if(!b){status=STATUS_INSUFFICIENT_RESOURCES;goto leave;}
 RtlZeroMemory(b,sizeof(*b));b->a=a;b->count=p->NumAllocations;
 /* No reference changes or output handles until ALL entries are admitted. */
 for(i=0;i<b->count;i++){
  for(j=0;j<i;j++)if(p->pOpenAllocation[i].hAllocation==p->pOpenAllocation[j].hAllocation){status=STATUS_INVALID_PARAMETER;goto abort;}
  if(!a->get_handle_data){status=STATUS_DEVICE_NOT_READY;goto abort;}
  RtlZeroMemory(&query,sizeof(query));query.hObject=p->pOpenAllocation[i].hAllocation;query.Type=DXGK_HANDLE_ALLOCATION;
  allocation=a->get_handle_data(&query);
  for(scan=a->engine.allocations;scan&&scan!=allocation;scan=scan->next){}
  if(!scan){status=STATUS_INVALID_PARAMETER;goto abort;}
  for(j=0;j<i;j++)if(b->allocations[j]==allocation){status=STATUS_INVALID_PARAMETER;goto abort;}
  if(allocation->shared.magic){
   struct geo_shared_surface resource_data,allocation_data;
   if(!p->pPrivateDriverData||p->PrivateDriverSize!=sizeof(resource_data)||!p->pOpenAllocation[i].pPrivateDriverData||
      p->pOpenAllocation[i].PrivateDriverDataSize!=sizeof(allocation_data)){status=STATUS_INVALID_PARAMETER;goto abort;}
   RtlCopyMemory(&resource_data,p->pPrivateDriverData,sizeof(resource_data));
   RtlCopyMemory(&allocation_data,p->pOpenAllocation[i].pPrivateDriverData,sizeof(allocation_data));
   if(!geo_shared_surface_equal(&allocation->shared,&resource_data)||!geo_shared_surface_equal(&allocation->shared,&allocation_data)){status=STATUS_INVALID_PARAMETER;goto abort;}
  }else if(p->PrivateDriverSize){status=STATUS_INVALID_PARAMETER;goto abort;}
  b->allocations[i]=allocation;
 }
 /* All inputs are valid. Prepare every open without changing live references. */
 for(i=0;i<b->count;i++){
  b->opens[i]=ExAllocatePoolWithTag(NonPagedPool,sizeof(*b->opens[i]),'oOeG');
  if(!b->opens[i]){status=STATUS_INSUFFICIENT_RESOURCES;goto abort;}
  RtlZeroMemory(b->opens[i],sizeof(*b->opens[i]));b->opens[i]->allocation=b->allocations[i];b->opens[i]->readonly=p->Flags.ReadOnly;
 }
 if(!a->synchronize||!a->synchronize(a->sync_context,allocation_open_commit,b)){status=STATUS_DEVICE_NOT_READY;goto abort;}
 status=b->status;if(status!=STATUS_SUCCESS)goto abort;
 for(i=0;i<b->count;i++){
  b->opens[i]->next=d->opens;d->opens=b->opens[i];
  p->pOpenAllocation[i].hDeviceSpecificAllocation=b->opens[i];
 }
 ExFreePoolWithTag(b,'bAeG');goto leave;
abort:
 for(i=0;i<b->count;i++)if(b->opens[i])ExFreePoolWithTag(b->opens[i],'oOeG');
 ExFreePoolWithTag(b,'bAeG');
leave:
 GeoSessionLeave(a,1);return status;
}
static BOOLEAN allocation_close_commit(void *arg)
{
 struct allocation_open_batch *b=arg;UINT i,j,n;
 b->status=STATUS_SUCCESS;
 for(i=0;i<b->count;i++){
  struct geo_open_allocation *o=b->opens[i];struct geo_allocation *x;
  for(x=b->a->engine.allocations;x&&x!=o->allocation;x=x->next){}
  if(!x){b->status=STATUS_INVALID_PARAMETER;return TRUE;}
  n=0;for(j=0;j<b->count;j++)if(b->opens[j]->allocation==x)n++;
  if(x->open_count<n){b->status=STATUS_INVALID_PARAMETER;return TRUE;}
  for(j=0;j<GEO_MAX_JOBS;j++){
   const struct geo_job *job=&b->a->engine.jobs[j];
   if(job->state&&job->state!=4&&(job->open_handle==o||job->vertex_open_handle==o||job->texture_open_handle==o)){
    b->status=STATUS_DEVICE_BUSY;return TRUE;
   }
  }
 }
 for(i=0;i<b->count;i++)b->opens[i]->allocation->open_count--;
 return TRUE;
}
NTSTATUS APIENTRY GeoCloseAllocation(HANDLE handle,const DXGKARG_CLOSEALLOCATION *p) {
 struct geo_kmd_device *d=handle;struct geo_kmd_adapter *a;struct geo_open_allocation **link,*o;
 struct allocation_open_batch *b;UINT i,j;NTSTATUS status;
 if(!d||!p||!p->NumAllocations||p->NumAllocations>GEO_ALLOCATION_BATCH_MAX||!p->pOpenHandleList)return STATUS_INVALID_PARAMETER;
 a=d->adapter;if(!GeoSessionEnter(a,2))return STATUS_DEVICE_NOT_READY;
 b=ExAllocatePoolWithTag(NonPagedPool,sizeof(*b),'bAeG');if(!b){status=STATUS_INSUFFICIENT_RESOURCES;goto leave;}
 RtlZeroMemory(b,sizeof(*b));b->a=a;b->count=p->NumAllocations;
 for(i=0;i<b->count;i++){
  for(j=0;j<i;j++)if(p->pOpenHandleList[i]==p->pOpenHandleList[j]){status=STATUS_INVALID_PARAMETER;goto done;}
  for(o=d->opens;o&&o!=p->pOpenHandleList[i];o=o->next){}
  if(!o){status=STATUS_INVALID_PARAMETER;goto done;}b->opens[i]=o;
 }
 if(!a->synchronize||!a->synchronize(a->sync_context,allocation_close_commit,b)){status=STATUS_DEVICE_NOT_READY;goto done;}
 status=b->status;if(status!=STATUS_SUCCESS)goto done;
 for(i=0;i<b->count;i++){
  for(link=&d->opens;*link!=b->opens[i];link=&(*link)->next){}
  *link=b->opens[i]->next;ExFreePoolWithTag(b->opens[i],'oOeG');
 }
done:
 ExFreePoolWithTag(b,'bAeG');
leave:
 GeoSessionLeave(a,2);return status;
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
 if(p->StandardAllocationType==D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE&&A04_GE8300_UNIFIED_DISPLAY356){
  struct geo_allocation_data_v2 primary;
  status=GeoDisplay356PrimaryLayout(a,p->pCreateSharedPrimarySurfaceData,&primary);if(status!=STATUS_SUCCESS)return status;
  /* WDK8.1: サイズ欄はout。OSが確保したfillバッファの入力capacityではない。 */
  p->AllocationPrivateDriverDataSize=sizeof(primary);p->ResourcePrivateDriverDataSize=0;
  if(p->pAllocationPrivateDriverData)RtlCopyMemory(p->pAllocationPrivateDriverData,&primary,sizeof(primary));return STATUS_SUCCESS;
 }
 if(p->StandardAllocationType==D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE) {
  if(!p->pCreateShadowSurfaceData||(p->pCreateShadowSurfaceData->Format!=D3DDDIFMT_A8R8G8B8&&p->pCreateShadowSurfaceData->Format!=D3DDDIFMT_X8R8G8B8))return STATUS_NOT_SUPPORTED;
  d.width=p->pCreateShadowSurfaceData->Width;d.height=p->pCreateShadowSurfaceData->Height;d.format=(UINT)p->pCreateShadowSurfaceData->Format;
 } else if(p->StandardAllocationType==D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE) {
  if(!p->pCreateStagingSurfaceData)return STATUS_INVALID_PARAMETER;
  d.width=p->pCreateStagingSurfaceData->Width;d.height=p->pCreateStagingSurfaceData->Height;d.format=GEO_FORMAT_X8R8G8B8;
 } else return STATUS_NOT_SUPPORTED;
 /* Validate the multiplication before publishing Pitch or private data. */
 if(!d.width||!d.height||d.width>1024||d.height>(A04_GE8300_UNIFIED_DISPLAY356?1280u:1024u))return STATUS_NOT_SUPPORTED;
 d.pitch=d.width*4u;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 if(!geo_desc_valid(&d)){status=STATUS_NOT_SUPPORTED;goto done;}
 RtlZeroMemory(&data,sizeof(data));data.size=sizeof(data);data.version=1;data.logical_width=d.width;data.logical_height=d.height;data.storage=d;
 p->AllocationPrivateDriverDataSize=sizeof(data);p->ResourcePrivateDriverDataSize=0;
 /* The NULL-buffer size query must leave the standard descriptor alone.
  * Publish its pitch only when filling the private allocation data. */
 if(p->pAllocationPrivateDriverData) {
  if(p->StandardAllocationType==D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE)p->pCreateStagingSurfaceData->Pitch=d.pitch;
  else p->pCreateShadowSurfaceData->Pitch=d.pitch;
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
  if(e->jobs[i].state==3||e->jobs[i].state==4||e->jobs[i].state==7||e->jobs[i].state==8||e->jobs[i].state==GEO_JOB_COPY_CONTINUE)c->snapshot.pinned++;
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
