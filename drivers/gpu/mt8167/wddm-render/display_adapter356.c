/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "display_adapter356.h"
#include "lifecycle.h"
#include "../wddm-display/unified_display.h"
#include "../display-transport/interface356.h"
#define TAG '6PvG'
struct display_map356 {struct display_map356 *next;struct geo_allocation *allocation;void *cookie;};
struct display_adapter356 {
 struct geo_kmd_adapter *adapter;void *bdd;PFILE_OBJECT provider_file;
 struct a04_display_transport_v1 transport;struct display_map356 *maps,*active,*selected;
 DXGKRNL_INTERFACE dxgk;LONG vsync_enabled,scanout_active;UINT started,subscribed;volatile LONG primary_offset_pages;
};
static struct display_adapter356 *display(struct geo_kmd_adapter *a){return a?a->display:NULL;}
void *GeoUnifiedDisplayContext(HANDLE a){struct display_adapter356 *d=display(a);return d?d->bdd:NULL;}
static NTSTATUS query_completed(PDEVICE_OBJECT device,PIRP irp,void *event){UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(irp);KeSetEvent(event,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS find_transport(struct display_adapter356 *d){
 PWSTR links=NULL;PDEVICE_OBJECT target=NULL;PIRP irp;PIO_STACK_LOCATION sp;KEVENT event;NTSTATUS status;
 status=IoGetDeviceInterfaces(&A04DisplayTransport356Guid,NULL,0,&links);if(!NT_SUCCESS(status))return status;
 if(!links||!links[0]){if(links)ExFreePool(links);return STATUS_DEVICE_NOT_READY;}
 /* Multiple owners would make the standard display route ambiguous. */
 if(links[wcslen(links)+1]){ExFreePool(links);return STATUS_OBJECT_NAME_COLLISION;}
 UNICODE_STRING name;RtlInitUnicodeString(&name,links);status=IoGetDeviceObjectPointer(&name,FILE_READ_DATA,&d->provider_file,&target);ExFreePool(links);if(!NT_SUCCESS(status))return status;
 irp=IoAllocateIrp(target->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 KeInitializeEvent(&event,NotificationEvent,FALSE);irp->RequestorMode=KernelMode;irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
 sp=IoGetNextIrpStackLocation(irp);sp->MajorFunction=IRP_MJ_PNP;sp->MinorFunction=IRP_MN_QUERY_INTERFACE;
 sp->Parameters.QueryInterface.InterfaceType=&A04DisplayTransport356Guid;sp->Parameters.QueryInterface.Size=sizeof(d->transport);sp->Parameters.QueryInterface.Version=1;
 sp->Parameters.QueryInterface.Interface=(PINTERFACE)&d->transport;sp->Parameters.QueryInterface.InterfaceSpecificData=NULL;
 IoSetCompletionRoutine(irp,query_completed,&event,TRUE,TRUE,TRUE);status=IoCallDriver(target,irp);if(status==STATUS_PENDING)KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
 status=irp->IoStatus.Status;IoFreeIrp(irp);
 if(!NT_SUCCESS(status))return status;
 if(d->transport.Header.Size!=sizeof(d->transport)||d->transport.Header.Version!=1||!d->transport.Header.Context||!d->transport.Header.InterfaceDereference||
    !d->transport.Map||!d->transport.Unmap||!d->transport.Select||!d->transport.Restore||!d->transport.Subscribe||!d->transport.Unsubscribe||!d->transport.BrightnessResources||!d->transport.Snapshot||!d->transport.Raster||!d->transport.EmergencyRestore)return STATUS_NOINTERFACE;
 return STATUS_SUCCESS;
}
static void vsync(void *context,ULONGLONG serial){
 struct display_adapter356 *d=context;DXGKARGCB_NOTIFY_INTERRUPT_DATA n;UNREFERENCED_PARAMETER(serial);
 if(!d->started||!InterlockedCompareExchange(&d->scanout_active,0,0)||!InterlockedCompareExchange(&d->vsync_enabled,0,0))return;
 RtlZeroMemory(&n,sizeof(n));n.InterruptType=DXGK_INTERRUPT_CRTC_VSYNC;n.CrtcVsync.VidPnTargetId=0;
 /* Source is an aperture segment: report its segment-relative primary offset,
  * not DIS0's DMA address or POST's CPU physical address. */
 n.CrtcVsync.PhysicalAddress.QuadPart=(ULONGLONG)(ULONG)InterlockedCompareExchange(&d->primary_offset_pages,0,0)*PAGE_SIZE;
 d->dxgk.DxgkCbNotifyInterrupt(d->dxgk.DeviceHandle,&n);
 if(!InterlockedCompareExchange(&d->adapter->dpc_pending,1,0))d->dxgk.DxgkCbQueueDpc(d->dxgk.DeviceHandle);
}
NTSTATUS GeoDisplay356Start(struct geo_kmd_adapter *a,PDEVICE_OBJECT pdo,DXGK_START_INFO *start,DXGKRNL_INTERFACE *dxgk,ULONG *sources,ULONG *children){
 struct display_adapter356 *d;struct a04_display_transport_snapshot snap;DXGK_DISPLAY_INFORMATION info;NTSTATUS status;ULONG v=0,c=0;PCM_RESOURCE_LIST brightness=NULL;
 if(!a||a->display||!pdo||!start||!dxgk||!sources||!children||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *sources=*children=0;d=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*d),TAG);if(!d)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(d,sizeof(*d));d->adapter=a;d->dxgk=*dxgk;a->display=d; /* Retain partial ownership for a real failure cleanup. */
 status=find_transport(d);if(!NT_SUCCESS(status))return status;
 RtlZeroMemory(&snap,sizeof(snap));snap.size=sizeof(snap);status=d->transport.Snapshot(d->transport.Header.Context,&snap);
 if(!NT_SUCCESS(status)||!snap.ready||snap.width!=800||snap.height!=1280||snap.pitch!=3200||snap.format!=21)return STATUS_DEVICE_CONFIGURATION_ERROR;
 status=GeoDisplayCreate(pdo,&d->bdd);if(!NT_SUCCESS(status))return status;
 status=d->transport.BrightnessResources(d->transport.Header.Context,&brightness);if(status!=STATUS_SUCCESS)return status;
 status=GeoDisplaySetBrightnessResources(d->bdd,brightness);if(status!=STATUS_SUCCESS)return status;
 status=GeoDisplayStart(d->bdd,start,dxgk,&v,&c);if(!NT_SUCCESS(status))return status;
 d->started=1;status=GeoDisplayPostInfo(d->bdd,&info);
 if(!NT_SUCCESS(status)||v!=1||c!=1||info.Width!=800||info.Height!=1280||info.Pitch!=3200||(info.ColorFormat!=D3DDDIFMT_A8R8G8B8&&info.ColorFormat!=D3DDDIFMT_X8R8G8B8)||info.PhysicAddress.QuadPart!=(LONGLONG)snap.post_address)return STATUS_DEVICE_CONFIGURATION_ERROR;
 status=d->transport.Subscribe(d->transport.Header.Context,vsync,d);if(!NT_SUCCESS(status))return status;
 d->subscribed=1;*sources=v;*children=c;return STATUS_SUCCESS;
}
NTSTATUS GeoDisplay356PrimaryLayout(struct geo_kmd_adapter *a,const D3DKMDT_SHAREDPRIMARYSURFACEDATA *p,struct geo_allocation_data_v2 *out){
 struct display_adapter356 *d=display(a);struct geo_post_primary_layout_v1 layout;NTSTATUS status;
 if(!d||!d->started||!out)return STATUS_DEVICE_NOT_READY;
 status=GeoDisplaySharedPrimaryLayout(d->bdd,p,&layout);if(!NT_SUCCESS(status))return status;
 RtlZeroMemory(out,sizeof(*out));out->base.size=sizeof(*out);out->base.version=2;out->base.logical_width=layout.width;out->base.logical_height=layout.height;
 out->base.storage=(struct geo_surface_desc){sizeof(struct geo_surface_desc),1,layout.width,layout.height,layout.pitch,layout.format,GEO_USAGE_STAGING,0};
 out->flags=GEO_ALLOCATION_PRIMARY;return geo_desc_valid(&out->base.storage)?STATUS_SUCCESS:STATUS_NOT_SUPPORTED;
}
struct source_call {struct geo_kmd_adapter *a;HANDLE handle;struct geo_allocation *allocation;UINT busy,completed_fence,submitted_fence,completed_ever,submitted_ever;};
static BOOLEAN source_locked(void *p){
 struct source_call *c=p;struct geo_allocation *i;
 for(i=c->a->engine.allocations;i&&i!=c->handle;i=i->next){}
 c->allocation=i;c->busy=0;c->completed_fence=c->a->engine.last_completed;c->submitted_fence=c->a->engine.last_submitted;c->completed_ever=c->a->engine.completed_ever;c->submitted_ever=c->a->engine.submitted_ever;
 for(UINT n=0;n<GEO_MAX_JOBS;n++)if(c->a->engine.jobs[n].state&&c->a->engine.jobs[n].state!=4)c->busy=1;
 return TRUE;
}
static NTSTATUS wait_source(struct source_call *c){
 LARGE_INTEGER pause;ULONGLONG began=KeQueryInterruptTime();pause.QuadPart=-10000;
 for(UINT n=0;n<1000;n++){
  if(!c->a->synchronize(c->a->sync_context,source_locked,c))return STATUS_DEVICE_NOT_READY;
  if(!c->busy)return STATUS_SUCCESS;
  if(KeQueryInterruptTime()-began>=10000000u)return STATUS_IO_TIMEOUT;
  /* Retain this DDI's rundown ref and OS-owned allocation, but let the
   * existing PASSIVE owner worker complete accepted queue work. */
  KeReleaseMutex(&c->a->session->passive_lock,FALSE);(void)KeDelayExecutionThread(KernelMode,FALSE,&pause);
  KeWaitForSingleObject(&c->a->session->passive_lock,Executive,KernelMode,FALSE,NULL);
 }
 return STATUS_IO_TIMEOUT;
}
NTSTATUS APIENTRY GeoDisplay356SetSource(HANDLE adapter,const DXGKARG_SETVIDPNSOURCEADDRESS *p){
 struct geo_kmd_adapter *a=adapter;struct display_adapter356 *d=display(a);struct source_call call;struct display_map356 *map;
 struct geo_frontend_mdl_lease lease;NTSTATUS status;DXGK_SETVIDPNSOURCEADDRESS_FLAGS allowed={0};
 allowed.ModeChange=1;allowed.SharedPrimaryTransition=1;
 if(!d||!d->started||!p||p->VidPnSourceId||p->ContextCount||(p->Flags.Value&~allowed.Value)||(!p->Flags.ModeChange&&!p->Flags.SharedPrimaryTransition)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_NOT_SUPPORTED;
 if(!p->hAllocation){
  /* Only the OS-owned physical POST mode reassertion is accepted. Never
   * interpret a segment-relative GPU address as a POST physical address. */
  status=GeoDisplayValidatePostAddress(d->bdd,p);if(status!=STATUS_SUCCESS)return status;
  if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
  RtlZeroMemory(&call,sizeof(call));call.a=a;
  status=wait_source(&call);if(status==STATUS_SUCCESS){status=GeoDisplay356Restore(a);if(status==STATUS_SUCCESS)d->selected=NULL;}
  GeoSessionLeave(a,1);return status;
 }
 if(p->PrimarySegment!=1u)return STATUS_NOT_SUPPORTED;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&call,sizeof(call));call.a=a;call.handle=p->hAllocation;
 status=wait_source(&call);if(status!=STATUS_SUCCESS)goto done;
 if(!call.allocation||!call.allocation->primary||!call.allocation->bound||call.allocation->source_id||call.busy||
    p->PrimaryAddress.QuadPart!=(LONGLONG)(call.allocation->aperture_offset*PAGE_SIZE)){status=STATUS_INVALID_PARAMETER;goto done;}
 for(map=d->maps;map&&map->allocation!=call.allocation;map=map->next){}
 if(!map){
  map=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*map),TAG);if(!map){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
  RtlZeroMemory(map,sizeof(*map));map->allocation=call.allocation;
  status=GeoAcquireScanoutMap(a,call.allocation,&lease);if(!NT_SUCCESS(status)){ExFreePoolWithTag(map,TAG);goto done;}
  status=d->transport.Map(d->transport.Header.Context,&lease,&map->cookie);lease.release(lease.context,lease.mdl);
  if(!NT_SUCCESS(status)){ExFreePoolWithTag(map,TAG);goto done;}
  call.allocation->display_refs++;map->next=d->maps;d->maps=map;
 }
 InterlockedExchange(&d->scanout_active,0);
 status=d->transport.Select(d->transport.Header.Context,map->cookie);
 if(status==STATUS_SUCCESS){d->active=map;d->selected=map;InterlockedExchange(&d->primary_offset_pages,(LONG)map->allocation->aperture_offset);InterlockedExchange(&d->scanout_active,1);} /* Actual OVL + video-mode RDMA IRQ were observed. */
 done:GeoSessionLeave(a,1);return status;
}
NTSTATUS APIENTRY GeoDisplay356ControlInterrupt(HANDLE adapter,DXGK_INTERRUPT_TYPE type,BOOLEAN enable){
 struct display_adapter356 *d=display(adapter);if(!d||!d->started||type!=DXGK_INTERRUPT_CRTC_VSYNC)return STATUS_NOT_SUPPORTED;
 InterlockedExchange(&d->vsync_enabled,enable?1:0);return STATUS_SUCCESS;
}
NTSTATUS GeoDisplay356Restore(struct geo_kmd_adapter *a){
 struct display_adapter356 *d=display(a);NTSTATUS status;if(!d)return STATUS_SUCCESS;
 InterlockedExchange(&d->scanout_active,0);
 status=d->transport.Header.Context?(d->transport.Restore?d->transport.Restore(d->transport.Header.Context):STATUS_NOINTERFACE):STATUS_SUCCESS;
 if(status==STATUS_SUCCESS){d->active=NULL;InterlockedExchange(&d->primary_offset_pages,0);}return status;
}
NTSTATUS GeoDisplay356ReleaseMaps(struct geo_kmd_adapter *a){
 struct display_adapter356 *d=display(a);NTSTATUS status;struct display_map356 *m;
 if(!d)return STATUS_SUCCESS;
 /* Called only after the normal native owner stop/IRQ-rundown proof. */
 if(!a->session||(a->session->state!=GEO_SESSION_STOPPED&&a->session->state!=GEO_SESSION_HALTED&&a->session->state!=GEO_SESSION_SUSPENDED&&a->session->state!=GEO_SESSION_RESET)||d->active)return STATUS_DEVICE_BUSY;
 while((m=d->maps)!=NULL){
  status=d->transport.Unmap(d->transport.Header.Context,m->cookie);if(status!=STATUS_SUCCESS)return status;
  if(!m->allocation->display_refs)return STATUS_INVALID_DEVICE_STATE;
  m->allocation->display_refs--;if(d->selected==m)d->selected=NULL;d->maps=m->next;ExFreePoolWithTag(m,TAG);
 }
 return STATUS_SUCCESS;
}
NTSTATUS GeoDisplay356RetireAllocation(struct geo_kmd_adapter *a,struct geo_allocation *allocation){
 struct display_adapter356 *d=display(a);struct display_map356 **link,*m;struct source_call call;NTSTATUS status;
 if(!d||!allocation||!allocation->display_refs)return STATUS_SUCCESS;
 RtlZeroMemory(&call,sizeof(call));call.a=a;call.handle=allocation;
 status=wait_source(&call);if(status!=STATUS_SUCCESS)return status;
 if(!call.allocation)return STATUS_INVALID_PARAMETER;
 if(d->active&&d->active->allocation==allocation){status=GeoDisplay356Restore(a);if(status!=STATUS_SUCCESS)return status;}
 /* Display no-read alone cannot release a mapping while the GPU still owns
  * its pages. Retire the actual native graph, not a metadata-only fence. */
 status=GeoUnifiedDrainNativeForDisplay(a);if(status!=STATUS_SUCCESS)return status;
 for(link=&d->maps;*link&&(*link)->allocation!=allocation;link=&(*link)->next){}
 if(!(m=*link))return STATUS_INVALID_DEVICE_STATE;
 status=d->transport.Unmap(d->transport.Header.Context,m->cookie);if(status!=STATUS_SUCCESS)return status;
 *link=m->next;if(d->selected==m)d->selected=NULL;allocation->display_refs--;ExFreePoolWithTag(m,TAG);return STATUS_SUCCESS;
}
NTSTATUS GeoDisplay356Stop(struct geo_kmd_adapter *a){
 struct display_adapter356 *d=display(a);NTSTATUS status;if(!d)return STATUS_SUCCESS;
 if(d->maps||d->active)return STATUS_DEVICE_BUSY;
 if(d->subscribed){status=d->transport.Unsubscribe(d->transport.Header.Context);if(status!=STATUS_SUCCESS)return status;d->subscribed=0;}
 if(d->started){if(GeoDisplayStarted(d->bdd)){status=GeoDisplayStop(d->bdd);if(status!=STATUS_SUCCESS)return status;}d->started=0;}
 if(d->bdd){status=GeoDisplayDelete(d->bdd);if(status!=STATUS_SUCCESS)return status;d->bdd=NULL;}
 if(d->transport.Header.InterfaceDereference)d->transport.Header.InterfaceDereference(d->transport.Header.Context);
 if(d->provider_file)ObDereferenceObject(d->provider_file);ExFreePoolWithTag(d,TAG);a->display=NULL;return STATUS_SUCCESS;
}
NTSTATUS GeoUnifiedQuiesceForPostRelease(HANDLE adapter){
 struct geo_kmd_adapter *a=adapter;NTSTATUS status=GeoDisplay356Restore(a);if(status!=STATUS_SUCCESS)return status;
 status=GeoStopOwnerSession(a->session);if(status!=STATUS_SUCCESS)return status;return GeoDisplay356ReleaseMaps(a);
}

void GeoDisplay356EmergencyRestore(struct geo_kmd_adapter *a){
 struct display_adapter356 *d=display(a);if(d&&d->transport.EmergencyRestore){InterlockedExchange(&d->vsync_enabled,0);d->transport.EmergencyRestore(d->transport.Header.Context);}
}

NTSTATUS APIENTRY GeoDisplay356GetScanLine(HANDLE adapter,DXGKARG_GETSCANLINE *p){
 struct display_adapter356 *d=display(adapter);ULONG line;BOOLEAN blank;NTSTATUS status;
 if(!d||!d->started||!p||p->VidPnTargetId||!d->transport.Raster)return STATUS_INVALID_PARAMETER;
 status=d->transport.Raster(d->transport.Header.Context,&line,&blank);
 if(status==STATUS_SUCCESS){p->InVerticalBlank=blank;p->ScanLine=line;}return status;
}

#include "scanout_observe356.h"
static UINT hash356(const UCHAR *p,UINT n){UINT h=2166136261u;for(UINT i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
NTSTATUS APIENTRY GeoDisplay356Observe(HANDLE adapter,const DXGKARG_ESCAPE *p){
 struct geo_kmd_adapter *a=adapter;struct display_adapter356 *d=display(a);struct geo_scanout_observe356 *o;
 struct source_call call;struct geo_frontend_mdl_lease lease;struct a04_display_transport_snapshot snap;void *cpu;NTSTATUS status;
 if(!p||p->Flags.Value||!p->pPrivateDriverData||p->PrivateDriverDataSize!=sizeof(*o)||!d||!d->started||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 o=p->pPrivateDriverData;
 if(o->size!=sizeof(*o)||o->version!=1||o->magic!=GEO_SCANOUT_OBSERVE356_MAGIC||o->flags||o->reserved[0]||o->reserved[1])return STATUS_INVALID_PARAMETER;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 if(!d->active){status=STATUS_DEVICE_NOT_READY;goto done;}
 RtlZeroMemory(&call,sizeof(call));call.a=a;call.handle=d->active->allocation;
 status=wait_source(&call);if(status!=STATUS_SUCCESS)goto done;
 if(!call.completed_ever||!call.submitted_ever||call.completed_fence!=call.submitted_fence||!d->active||d->active->allocation!=call.allocation){status=STATUS_DEVICE_NOT_READY;goto done;}
 RtlZeroMemory(&snap,sizeof(snap));snap.size=sizeof(snap);status=d->transport.Snapshot(d->transport.Header.Context,&snap);
 if(status!=STATUS_SUCCESS||!snap.ready||!snap.frame_serial||snap.last_status!=STATUS_SUCCESS||!snap.active_address){status=STATUS_DEVICE_NOT_READY;goto done;}
 /* A frame interrupt must occur AFTER the genuine GPU completion boundary,
  * rather than reusing the Select mode-change's earlier frame IRQ. */
 {ULONGLONG before=snap.frame_serial,began=KeQueryInterruptTime();LARGE_INTEGER pause;pause.QuadPart=-10000;
  for(UINT n=0;n<100;n++){
   (void)KeDelayExecutionThread(KernelMode,FALSE,&pause);
   status=d->transport.Snapshot(d->transport.Header.Context,&snap);
   if(status!=STATUS_SUCCESS||!snap.ready||snap.last_status!=STATUS_SUCCESS){status=STATUS_DEVICE_NOT_READY;goto done;}
   if(snap.frame_serial>before)break;
   if(n==99||KeQueryInterruptTime()-began>=1000000u){status=STATUS_IO_TIMEOUT;goto done;}
  }
 }
 status=GeoAcquireScanoutMap(a,call.allocation,&lease);if(status!=STATUS_SUCCESS)goto done;
 cpu=MmGetSystemAddressForMdlSafe(lease.mdl,NormalPagePriority);
 if(!cpu){status=STATUS_INSUFFICIENT_RESOURCES;goto release;}
 /* The same passive_lock excludes the sole real prepare/submit worker.
  * wait_source proved all accepted work retired; this is an invalidate/read,
  * never a CPU write into the display or a replacement rendering path. */
 KeFlushIoBuffers(lease.mdl,TRUE,TRUE);KeMemoryBarrier();
 RtlCopyMemory(o->pixels,cpu,GEO_SCANOUT_OBSERVE356_BYTES);
 o->width=800;o->height=1280;o->pitch=3200;o->bytes=GEO_SCANOUT_OBSERVE356_BYTES;
 o->frame_low=(UINT)snap.frame_serial;o->frame_high=(UINT)(snap.frame_serial>>32);
 o->completed_fence=call.completed_fence;o->submitted_fence=call.submitted_fence;
 o->pixel_hash=hash356(o->pixels,o->bytes);o->pfn_hash=hash356((const UCHAR *)MmGetMdlPfnArray(lease.mdl),1000u*sizeof(PFN_NUMBER));
 o->flags=3u;status=STATUS_SUCCESS; /* Actual selected backing + real frame IRQ. */
 release:lease.release(lease.context,lease.mdl);
 done:GeoSessionLeave(a,1);return status;
}

NTSTATUS GeoDisplay356Visibility(HANDLE adapter,const DXGKARG_SETVIDPNSOURCEVISIBILITY *p){
 struct geo_kmd_adapter *a=adapter;struct display_adapter356 *d=display(a);NTSTATUS status;
 if(!d||!p||(p->VidPnSourceId&&p->VidPnSourceId!=D3DDDI_ID_ALL)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 if(!p->Visible)status=GeoDisplay356Restore(a);
 else if(d->selected){
  InterlockedExchange(&d->scanout_active,0);
  status=d->transport.Select(d->transport.Header.Context,d->selected->cookie);
  if(status==STATUS_SUCCESS){d->active=d->selected;InterlockedExchange(&d->primary_offset_pages,(LONG)d->active->allocation->aperture_offset);InterlockedExchange(&d->scanout_active,1);}
 }else status=STATUS_SUCCESS; /* POST remains the OS-owned boot surface. */
 GeoSessionLeave(a,1);return status;
}
