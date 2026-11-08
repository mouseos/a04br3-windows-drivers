/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#if !defined(A04_MT8167_DISPLAY_BRIGHTNESS) || A04_MT8167_DISPLAY_BRIGHTNESS != 1
#error Compile the unified bridge and all DOD1.7 units with brightness=1
#endif
#include "bdd.hxx"
#include "unified_display.h"
/* These legacy paths create a second provider/native owner. A unified
 * display build must use the render miniport's existing native owner. */
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT || A04_GE8300_WDDM_SHARED_AUTOSTART || A04_GE8300_WDDM_VENDOR_TILE || A04_GE8300_WDDM_COPY
#error Disable legacy DOD GPU-provider experiments for unified display
#endif
struct geo_display { BASIC_DISPLAY_DRIVER *bdd;BOOLEAN started;UINT read_rundown_closed;EX_RUNDOWN_REF read_operations; };
static geo_display *state(void *p){return static_cast<geo_display *>(p);}
static BASIC_DISPLAY_DRIVER *bdd(void *p){geo_display *s=state(p);return s&&s->started?s->bdd:NULL;}
static BASIC_DISPLAY_DRIVER *from_adapter(HANDLE p){return bdd(GeoUnifiedDisplayContext(p));}
extern "C" NTSTATUS GeoDisplayCreate(PDEVICE_OBJECT pdo,void **out) {
 if(!pdo||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *out=NULL;geo_display *s=static_cast<geo_display *>(ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*s),'uDgG'));
 if(!s)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(s,sizeof(*s));ExInitializeRundownProtection(&s->read_operations);s->bdd=new(BDD_POOL_TYPE::NonPaged) BASIC_DISPLAY_DRIVER(pdo);
 if(!s->bdd){ExFreePoolWithTag(s,'uDgG');return STATUS_INSUFFICIENT_RESOURCES;}
 *out=s;return STATUS_SUCCESS;
}
extern "C" NTSTATUS GeoDisplayStart(void *handle,DXGK_START_INFO *start,DXGKRNL_INTERFACE *dxgk,ULONG *sources,ULONG *children) {
 geo_display *s=state(handle);ULONG v=0,c=0;NTSTATUS status;
 if(!s||s->started||!start||!dxgk||!sources||!children||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *sources=0;*children=0;
 status=s->bdd->StartDevice(start,dxgk,&v,&c);
 if(!NT_SUCCESS(status))return status;
 if(s->read_rundown_closed){ExReInitializeRundownProtection(&s->read_operations);s->read_rundown_closed=0;}
 s->started=TRUE;
 /* BDD acquired the actual POST owner and validated its framebuffer layout.
  * Counts are published only after that succeeds, never synthesised here. */
 *sources=v;*children=c;return STATUS_SUCCESS;
}
extern "C" NTSTATUS GeoDisplayStop(void *handle) {
 geo_display *s=state(handle);NTSTATUS status;
 if(!s||!s->started||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 if(!s->read_rundown_closed){ExWaitForRundownProtectionRelease(&s->read_operations);s->read_rundown_closed=1;}
 status=s->bdd->StopDevice();if(NT_SUCCESS(status))s->started=FALSE;return status;
}
extern "C" NTSTATUS GeoDisplayDelete(void *handle) {
 geo_display *s=state(handle);if(!s||s->started||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_DEVICE_BUSY;
 delete s->bdd;ExFreePoolWithTag(s,'uDgG');return STATUS_SUCCESS;
}
extern "C" NTSTATUS GeoDisplayPostInfo(void *handle,DXGK_DISPLAY_INFORMATION *info) {
 BASIC_DISPLAY_DRIVER *d=bdd(handle);const CURRENT_BDD_MODE *mode;
 if(!d||!info||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_DEVICE_NOT_READY;
 mode=d->GetCurrentMode(0);if(!mode||!mode->DispInfo.Width||!mode->DispInfo.Height)return STATUS_DEVICE_NOT_READY;
 *info=mode->DispInfo;return STATUS_SUCCESS;
}
extern "C" NTSTATUS GeoDisplaySharedPrimaryLayout(void *h,const D3DKMDT_SHAREDPRIMARYSURFACEDATA *request,struct geo_post_primary_layout_v1 *out) {
 DXGK_DISPLAY_INFORMATION info;NTSTATUS status;
 if(!request||!out||request->VidPnSourceId!=0||request->Format!=D3DDDIFMT_A8R8G8B8)return STATUS_NOT_SUPPORTED;
 RtlZeroMemory(out,sizeof(*out));status=GeoDisplayPostInfo(h,&info);if(!NT_SUCCESS(status))return status;
 /* Native primary rendering at rotated dimensions is a separate contract.
  * Preserve DOD1.7 rotation, but never widen native caps from a mode list. */
 if(info.ColorFormat!=request->Format||request->Width!=info.Width||request->Height!=info.Height||
    info.Width>MAXULONG/4u||info.Pitch<info.Width*4u||(info.Pitch&3u)||
    (ULONGLONG)info.Height*info.Pitch>MAXULONG||
    (ULONGLONG)info.PhysicAddress.QuadPart>MAXULONGLONG-(ULONGLONG)info.Height*info.Pitch)return STATUS_NOT_SUPPORTED;
 out->size=sizeof(*out);out->version=1;out->source_id=0;out->width=info.Width;out->height=info.Height;
 out->pitch=info.Pitch;out->format=(UINT)info.ColorFormat;return STATUS_SUCCESS;
}
extern "C" NTSTATUS GeoDisplayValidatePostAddress(void *h,const DXGKARG_SETVIDPNSOURCEADDRESS *p) {
 DXGK_DISPLAY_INFORMATION info;NTSTATUS status;
 if(!p||p->VidPnSourceId||p->PrimarySegment||p->hAllocation||p->ContextCount||p->Flags.Value!=1u)return STATUS_NOT_SUPPORTED;
 status=GeoDisplayPostInfo(h,&info);if(!NT_SUCCESS(status))return status;
 /* Only a physical, no-flip mode reassertion of the real POST address.
  * Returning success here does not claim a new scanout allocation or vblank. */
 return p->PrimaryAddress.QuadPart==info.PhysicAddress.QuadPart?STATUS_SUCCESS:STATUS_NOT_SUPPORTED;
}
extern "C" NTSTATUS GeoDisplayPresentCompleted(void *h,const struct geo_present_read_ops_v1 *ops,ULONGLONG epoch,ULONGLONG job,UINT generation) {
 geo_display *s=state(h);struct geo_present_read_lease_v1 view={};NTSTATUS status,released;
 if(!s||KeGetCurrentIrql()!=PASSIVE_LEVEL||!epoch||!job||!generation||!ops||
    ops->size!=sizeof(*ops)||ops->version!=1||!ops->context||!ops->acquire||!ops->release)return STATUS_INVALID_PARAMETER;
 if(!ExAcquireRundownProtection(&s->read_operations))return STATUS_DEVICE_NOT_READY;
 if(!s->started){ExReleaseRundownProtection(&s->read_operations);return STATUS_DEVICE_NOT_READY;}
 view.size=sizeof(view);view.version=1;
 status=ops->acquire(ops->context,epoch,job,generation,&view);
 if(status==STATUS_SUCCESS){
  if(view.size!=sizeof(view)||view.version!=1||view.reserved||!view.cookie||!view.mdl)status=STATUS_INVALID_DEVICE_STATE;
  else status=s->bdd->PresentCompletedStaging(view.mdl,view.width,view.height,view.pitch);
 }else if(NT_SUCCESS(status))status=STATUS_DEVICE_NOT_READY; // PENDING is not completed CPU ownership.
 // Even a malformed provider failure must not strand a returned read lease.
 if(view.cookie){
  released=ops->release(ops->context,view.cookie);
  if(status==STATUS_SUCCESS&&released!=STATUS_SUCCESS)status=NT_SUCCESS(released)?STATUS_INVALID_DEVICE_STATE:released;
 }
 ExReleaseRundownProtection(&s->read_operations);return status;
}
extern "C" NTSTATUS GeoDisplayPower(void *h,ULONG id,DEVICE_POWER_STATE p,POWER_ACTION a) {
 BASIC_DISPLAY_DRIVER *d=bdd(h);return d?d->SetPowerState(id,p,a):STATUS_DEVICE_NOT_READY;
}
#define GEO_WRAP(name,type,method) static NTSTATUS name(HANDLE h,type *arg){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->method(arg):STATUS_DEVICE_NOT_READY;}
#define GEO_WRAP_CONST(name,type,method) static NTSTATUS name(HANDLE h,const type *arg){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->method(arg):STATUS_DEVICE_NOT_READY;}
GEO_WRAP(supported,DXGKARG_ISSUPPORTEDVIDPN,IsSupportedVidPn)
GEO_WRAP_CONST(recommend,DXGKARG_RECOMMENDFUNCTIONALVIDPN,RecommendFunctionalVidPn)
GEO_WRAP_CONST(enum_modes,DXGKARG_ENUMVIDPNCOFUNCMODALITY,EnumVidPnCofuncModality)
GEO_WRAP_CONST(visibility,DXGKARG_SETVIDPNSOURCEVISIBILITY,SetVidPnSourceVisibility)
GEO_WRAP_CONST(commit,DXGKARG_COMMITVIDPN,CommitVidPn)
GEO_WRAP_CONST(update,DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH,UpdateActiveVidPnPresentPath)
GEO_WRAP_CONST(monitor,DXGKARG_RECOMMENDMONITORMODES,RecommendMonitorModes)
GEO_WRAP(hwcap,DXGKARG_QUERYVIDPNHWCAPABILITY,QueryVidPnHWCapability)
GEO_WRAP_CONST(pointer_pos,DXGKARG_SETPOINTERPOSITION,SetPointerPosition)
GEO_WRAP_CONST(pointer_shape,DXGKARG_SETPOINTERSHAPE,SetPointerShape)
static NTSTATUS child_relations(HANDLE h,DXGK_CHILD_DESCRIPTOR *p,ULONG bytes){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->QueryChildRelations(p,bytes):STATUS_DEVICE_NOT_READY;}
static NTSTATUS child_status(HANDLE h,DXGK_CHILD_STATUS *p,BOOLEAN nondestructive){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->QueryChildStatus(p,nondestructive):STATUS_DEVICE_NOT_READY;}
static NTSTATUS descriptor(HANDLE h,ULONG id,DXGK_DEVICE_DESCRIPTOR *p){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->QueryDeviceDescriptor(id,p):STATUS_DEVICE_NOT_READY;}
static NTSTATUS dispatch(HANDLE h,ULONG source,PVIDEO_REQUEST_PACKET p){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->DispatchIoRequest(source,p):STATUS_DEVICE_NOT_READY;}
static NTSTATUS release_post(HANDLE h,D3DDDI_VIDEO_PRESENT_TARGET_ID target,DXGK_DISPLAY_INFORMATION *p){
 geo_display *s=state(GeoUnifiedDisplayContext(h));BASIC_DISPLAY_DRIVER *d=bdd(s);
 if(!d)return STATUS_DEVICE_NOT_READY;
 if(!s->read_rundown_closed){ExWaitForRundownProtectionRelease(&s->read_operations);s->read_rundown_closed=1;}
 NTSTATUS status=d->StopDeviceAndReleasePostDisplayOwnership(target,p);
 if(NT_SUCCESS(status))s->started=FALSE; /* POST already stopped; no double Stop. */
 return status;
}
static void reset_display(HANDLE h){BASIC_DISPLAY_DRIVER *d=from_adapter(h);if(d)d->ResetDevice();}
static NTSTATUS system_enable(HANDLE h,D3DDDI_VIDEO_PRESENT_TARGET_ID target,PDXGKARG_SYSTEM_DISPLAY_ENABLE_FLAGS flags,UINT *w,UINT *height,D3DDDIFORMAT *format){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->SystemDisplayEnable(target,flags,w,height,format):STATUS_DEVICE_NOT_READY;}
static void system_write(HANDLE h,void *source,UINT w,UINT height,UINT stride,UINT x,UINT y){BASIC_DISPLAY_DRIVER *d=from_adapter(h);if(d)d->SystemDisplayWrite(source,w,height,stride,x,y);}
extern "C" NTSTATUS GeoDisplayFillVidPnCallbacks(DRIVER_INITIALIZATION_DATA *t) {
 if(!t)return STATUS_INVALID_PARAMETER;
 t->DxgkDdiIsSupportedVidPn=supported;t->DxgkDdiRecommendFunctionalVidPn=recommend;
 t->DxgkDdiEnumVidPnCofuncModality=enum_modes;t->DxgkDdiSetVidPnSourceVisibility=visibility;
 t->DxgkDdiCommitVidPn=commit;t->DxgkDdiUpdateActiveVidPnPresentPath=update;
 t->DxgkDdiRecommendMonitorModes=monitor;t->DxgkDdiQueryVidPnHWCapability=hwcap;
 t->DxgkDdiSetPointerPosition=pointer_pos;t->DxgkDdiSetPointerShape=pointer_shape;
 t->DxgkDdiQueryChildRelations=child_relations;t->DxgkDdiQueryChildStatus=child_status;
 t->DxgkDdiQueryDeviceDescriptor=descriptor;t->DxgkDdiDispatchIoRequest=dispatch;
 t->DxgkDdiStopDeviceAndReleasePostDisplayOwnership=release_post;
 t->DxgkDdiResetDevice=reset_display;t->DxgkDdiSystemDisplayEnable=system_enable;t->DxgkDdiSystemDisplayWrite=system_write;
 return STATUS_SUCCESS;
}
