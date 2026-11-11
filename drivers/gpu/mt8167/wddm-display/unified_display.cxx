/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#if !defined(A04_MT8167_DISPLAY_BRIGHTNESS) || A04_MT8167_DISPLAY_BRIGHTNESS != 1
#error Compile the unified bridge and all DOD1.7 units with brightness=1
#endif
#include "bdd.hxx"
#include "unified_display.h"
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define GEO_POST366_RETURN_ADDRESS() _ReturnAddress()
#else
#define GEO_POST366_RETURN_ADDRESS() __builtin_return_address(0)
#endif
/* These legacy paths create a second provider/native owner. A unified
 * display build must use the render miniport's existing native owner. */
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT || A04_GE8300_WDDM_SHARED_AUTOSTART || A04_GE8300_WDDM_VENDOR_TILE || A04_GE8300_WDDM_COPY
#error Disable legacy DOD GPU-provider experiments for unified display
#endif
#include "vidpn_callback_trace.h"
struct geo_display { geo_vidpn_trace_state trace;BASIC_DISPLAY_DRIVER *bdd;BOOLEAN started;UINT read_rundown_closed;EX_RUNDOWN_REF read_operations; };
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
 GeoVidPnTraceEpoch(&s->trace,STATUS_PENDING,FALSE);
 status=s->bdd->StartDevice(start,dxgk,&v,&c);
 GeoVidPnTraceEpoch(&s->trace,status,TRUE);
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
extern "C" BOOLEAN GeoDisplayStarted(void *p){geo_display *s=state(p);return s&&s->started;}
extern "C" NTSTATUS GeoDisplayPostInfo(void *handle,DXGK_DISPLAY_INFORMATION *info) {
 BASIC_DISPLAY_DRIVER *d=bdd(handle);const CURRENT_BDD_MODE *mode;
 if(!d||!info||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_DEVICE_NOT_READY;
 mode=d->GetCurrentMode(0);if(!mode||!mode->DispInfo.Width||!mode->DispInfo.Height)return STATUS_DEVICE_NOT_READY;
 *info=mode->DispInfo;return STATUS_SUCCESS;
}
extern "C" NTSTATUS GeoDisplaySharedPrimaryLayout(void *h,const D3DKMDT_SHAREDPRIMARYSURFACEDATA *request,struct geo_post_primary_layout_v1 *out) {
 DXGK_DISPLAY_INFORMATION info;NTSTATUS status;
 if(!request||!out||request->VidPnSourceId!=0||(request->Format!=D3DDDIFMT_A8R8G8B8&&request->Format!=D3DDDIFMT_X8R8G8B8))return STATUS_NOT_SUPPORTED;
 RtlZeroMemory(out,sizeof(*out));status=GeoDisplayPostInfo(h,&info);if(!NT_SUCCESS(status))return status;
 /* Native primary rendering at rotated dimensions is a separate contract.
  * Preserve DOD1.7 rotation, but never widen native caps from a mode list. */
 if((info.ColorFormat!=D3DDDIFMT_A8R8G8B8&&info.ColorFormat!=D3DDDIFMT_X8R8G8B8)||request->Width!=info.Width||request->Height!=info.Height||
    info.Width>MAXULONG/4u||info.Pitch<info.Width*4u||(info.Pitch&3u)||
    (ULONGLONG)info.Height*info.Pitch>MAXULONG||
    (ULONGLONG)info.PhysicAddress.QuadPart>MAXULONGLONG-(ULONGLONG)info.Height*info.Pitch)return STATUS_NOT_SUPPORTED;
 out->size=sizeof(*out);out->version=1;out->source_id=0;out->width=info.Width;out->height=info.Height;
 out->pitch=info.Pitch;out->format=(UINT)request->Format;return STATUS_SUCCESS;
}
extern "C" NTSTATUS GeoDisplayValidatePostAddress(void *h,const DXGKARG_SETVIDPNSOURCEADDRESS *p) {
 DXGK_DISPLAY_INFORMATION info;NTSTATUS status;DXGK_SETVIDPNSOURCEADDRESS_FLAGS allowed={};allowed.ModeChange=1;allowed.SharedPrimaryTransition=1;
 if(!p||p->VidPnSourceId||p->PrimarySegment||p->hAllocation||p->ContextCount||(p->Flags.Value&~allowed.Value)||(!p->Flags.ModeChange&&!p->Flags.SharedPrimaryTransition))return STATUS_NOT_SUPPORTED;
 status=GeoDisplayPostInfo(h,&info);if(!NT_SUCCESS(status))return status;
 /* Only a physical, no-flip mode reassertion of the real POST address.
  * Returning success here does not claim a new scanout allocation or vblank. */
 return p->PrimaryAddress.QuadPart==info.PhysicAddress.QuadPart?STATUS_SUCCESS:STATUS_NOT_SUPPORTED;
}
extern "C" NTSTATUS GeoDisplayPower(void *h,ULONG id,DEVICE_POWER_STATE p,POWER_ACTION a) {
 BASIC_DISPLAY_DRIVER *d=bdd(h);return d?d->SetPowerState(id,p,a):STATUS_DEVICE_NOT_READY;
}
#define GEO_WRAP(name,type,method) static NTSTATUS name(HANDLE h,type *arg){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->method(arg):STATUS_DEVICE_NOT_READY;}
#define GEO_WRAP_CONST(name,type,method) static NTSTATUS name(HANDLE h,const type *arg){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->method(arg):STATUS_DEVICE_NOT_READY;}
static geo_display *trace_begin(HANDLE h,ULONG kind,geo_vidpn_trace_record *r){
 geo_display *s=state(GeoUnifiedDisplayContext(h));GeoVidPnTraceBegin(s?&s->trace:NULL,kind,r);GeoVidPnTraceMode(r,bdd(s),0);return s;
}
static NTSTATUS trace_end(geo_display *s,geo_vidpn_trace_record *r,NTSTATUS status){
 GeoVidPnTraceMode(r,bdd(s),12);return GeoVidPnTraceEnd(s?&s->trace:NULL,r,status);
}
static NTSTATUS supported(HANDLE h,DXGKARG_ISSUPPORTEDVIDPN *p){
 geo_vidpn_trace_record r;geo_display *s=trace_begin(h,1,&r);BASIC_DISPLAY_DRIVER *d=bdd(s);
 r.data[24]=p!=NULL;r.data[25]=p&&p->hDesiredVidPn!=0;
 GeoVidPnTraceModes(&r,d,p?p->hDesiredVidPn:0,32);GeoVidPnTraceEntry(&r);
 NTSTATUS status=d?d->IsSupportedVidPn(p):STATUS_DEVICE_NOT_READY;r.data[31]=d&&p&&NT_SUCCESS(status)?(ULONG)p->IsVidPnSupported:MAXULONG;
 GeoVidPnTraceModes(&r,d,p?p->hDesiredVidPn:0,60);return trace_end(s,&r,status);
}
static NTSTATUS recommend(HANDLE h,const DXGKARG_RECOMMENDFUNCTIONALVIDPN *p){
 geo_vidpn_trace_record r;geo_display *s=trace_begin(h,7,&r);BASIC_DISPLAY_DRIVER *d=bdd(s);
 if(p){r.data[24]=p->NumberOfVidPnTargets;r.data[25]=(ULONG)p->RequestReason;}
 GeoVidPnTraceModes(&r,d,p?p->hRecommendedFunctionalVidPn:0,32);GeoVidPnTraceEntry(&r);
 NTSTATUS status=d?d->RecommendFunctionalVidPn(p):STATUS_DEVICE_NOT_READY;
 GeoVidPnTraceModes(&r,d,p?p->hRecommendedFunctionalVidPn:0,60);return trace_end(s,&r,status);
}
static NTSTATUS enum_modes(HANDLE h,const DXGKARG_ENUMVIDPNCOFUNCMODALITY *p){
 geo_vidpn_trace_record r;geo_display *s=trace_begin(h,2,&r);BASIC_DISPLAY_DRIVER *d=bdd(s);
 if(p){r.data[24]=(ULONG)p->EnumPivotType;r.data[25]=p->EnumPivot.VidPnSourceId;r.data[26]=p->EnumPivot.VidPnTargetId;}
 GeoVidPnTraceModes(&r,d,p?p->hConstrainingVidPn:0,32);GeoVidPnTraceEntry(&r);
 NTSTATUS status=d?d->EnumVidPnCofuncModality(p):STATUS_DEVICE_NOT_READY;
 GeoVidPnTraceModes(&r,d,p?p->hConstrainingVidPn:0,60);return trace_end(s,&r,status);
}
extern "C" NTSTATUS GeoDisplay356Visibility(HANDLE,const DXGKARG_SETVIDPNSOURCEVISIBILITY *);
static NTSTATUS visibility(HANDLE h,const DXGKARG_SETVIDPNSOURCEVISIBILITY *p){
 geo_vidpn_trace_record r;geo_display *s=trace_begin(h,6,&r);BASIC_DISPLAY_DRIVER *d=bdd(s);
 if(p){r.data[24]=p->VidPnSourceId;r.data[25]=p->Visible;}GeoVidPnTraceEntry(&r);
 NTSTATUS status=(!d||!p)?STATUS_DEVICE_NOT_READY:d->SetVidPnSourceVisibility(p);
 if(status==STATUS_SUCCESS)status=GeoDisplay356Visibility(h,p);return trace_end(s,&r,status);
}
static NTSTATUS commit(HANDLE h,const DXGKARG_COMMITVIDPN *p){
 geo_vidpn_trace_record r;geo_display *s=trace_begin(h,4,&r);BASIC_DISPLAY_DRIVER *d=bdd(s);
 if(p){r.data[24]=p->AffectedVidPnSourceId;r.data[25]=p->Flags.PathPowerTransition;r.data[26]=p->Flags.PathPoweredOff;r.data[27]=(ULONG)p->MonitorConnectivityChecks;r.data[28]=p->hPrimaryAllocation!=NULL;}
 GeoVidPnTraceModes(&r,d,p?p->hFunctionalVidPn:0,32);GeoVidPnTraceEntry(&r);
 NTSTATUS status=d?d->CommitVidPn(p):STATUS_DEVICE_NOT_READY;
 GeoVidPnTraceModes(&r,d,p?p->hFunctionalVidPn:0,60);return trace_end(s,&r,status);
}
static NTSTATUS update(HANDLE h,const DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH *p){
 geo_vidpn_trace_record r;geo_display *s=trace_begin(h,5,&r);BASIC_DISPLAY_DRIVER *d=bdd(s);
 if(p){r.data[24]=p->VidPnPresentPathInfo.VidPnSourceId;r.data[25]=p->VidPnPresentPathInfo.VidPnTargetId;r.data[26]=(ULONG)p->VidPnPresentPathInfo.ContentTransformation.Rotation;r.data[27]=(ULONG)p->VidPnPresentPathInfo.ContentTransformation.Scaling;}
 GeoVidPnTraceEntry(&r);NTSTATUS status=d?d->UpdateActiveVidPnPresentPath(p):STATUS_DEVICE_NOT_READY;return trace_end(s,&r,status);
}
static NTSTATUS monitor(HANDLE h,const DXGKARG_RECOMMENDMONITORMODES *p){
 geo_vidpn_trace_record r;geo_display *s=trace_begin(h,3,&r);BASIC_DISPLAY_DRIVER *d=bdd(s);
 if(p)r.data[24]=p->VideoPresentTargetId;GeoVidPnTraceMonitor(&r,p,32);GeoVidPnTraceEntry(&r);
 NTSTATUS status=d?d->RecommendMonitorModes(p):STATUS_DEVICE_NOT_READY;
 GeoVidPnTraceMonitor(&r,p,60);return trace_end(s,&r,status);
}
GEO_WRAP(hwcap,DXGKARG_QUERYVIDPNHWCAPABILITY,QueryVidPnHWCapability)
GEO_WRAP_CONST(pointer_pos,DXGKARG_SETPOINTERPOSITION,SetPointerPosition)
GEO_WRAP_CONST(pointer_shape,DXGKARG_SETPOINTERSHAPE,SetPointerShape)
/* The RT8.1 full render+display interface requires SetPalette even when
 * its only exposed scanout mode is 32-bit true colour. DOD omitted it.
 * No indexed mode/CLUT is advertised, so never acknowledge a palette write.
 * REA dxgkrnl 2f0fe1db: 87534 tests adapter+EC when render interface 4BC
 * exists; 87BBC copies this slot from the registered SetPalette callback. */
C_ASSERT(FIELD_OFFSET(DRIVER_INITIALIZATION_DATA,DxgkDdiSetPalette)==0x74);
static NTSTATUS APIENTRY palette(HANDLE h,const DXGKARG_SETPALETTE *p){
 PAGED_CODE();
 if(!p||p->VidPnSourceId!=0)return STATUS_INVALID_PARAMETER;
 if(!from_adapter(h))return STATUS_DEVICE_NOT_READY;
 return STATUS_NOT_SUPPORTED;
}
static NTSTATUS child_relations(HANDLE h,DXGK_CHILD_DESCRIPTOR *p,ULONG bytes){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->QueryChildRelations(p,bytes):STATUS_DEVICE_NOT_READY;}
static NTSTATUS child_status(HANDLE h,DXGK_CHILD_STATUS *p,BOOLEAN nondestructive){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->QueryChildStatus(p,nondestructive):STATUS_DEVICE_NOT_READY;}
static NTSTATUS descriptor(HANDLE h,ULONG id,DXGK_DEVICE_DESCRIPTOR *p){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->QueryDeviceDescriptor(id,p):STATUS_DEVICE_NOT_READY;}
static NTSTATUS dispatch(HANDLE h,ULONG source,PVIDEO_REQUEST_PACKET p){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->DispatchIoRequest(source,p):STATUS_DEVICE_NOT_READY;}
static NTSTATUS release_post(HANDLE h,D3DDDI_VIDEO_PRESENT_TARGET_ID target,DXGK_DISPLAY_INFORMATION *p){
 GeoUnifiedRecordPostReleaseStack(h,GEO_POST366_RETURN_ADDRESS());
 geo_display *s=state(GeoUnifiedDisplayContext(h));BASIC_DISPLAY_DRIVER *d=bdd(s);
 if(!d)return STATUS_DEVICE_NOT_READY;
 if(!s->read_rundown_closed){ExWaitForRundownProtectionRelease(&s->read_operations);s->read_rundown_closed=1;}
 NTSTATUS status=GeoUnifiedQuiesceForPostRelease(h);if(status!=STATUS_SUCCESS)return status;
 status=d->StopDeviceAndReleasePostDisplayOwnership(target,p);
 if(NT_SUCCESS(status))s->started=FALSE; /* POST already stopped; no double Stop. */
 return status;
}
extern "C" void GeoUnifiedEmergencyRestore(HANDLE);
static void reset_display(HANDLE h){GeoUnifiedEmergencyRestore(h);BASIC_DISPLAY_DRIVER *d=from_adapter(h);if(d)d->ResetDevice();}
static NTSTATUS system_enable(HANDLE h,D3DDDI_VIDEO_PRESENT_TARGET_ID target,PDXGKARG_SYSTEM_DISPLAY_ENABLE_FLAGS flags,UINT *w,UINT *height,D3DDDIFORMAT *format){BASIC_DISPLAY_DRIVER *d=from_adapter(h);return d?d->SystemDisplayEnable(target,flags,w,height,format):STATUS_DEVICE_NOT_READY;}
static void system_write(HANDLE h,void *source,UINT w,UINT height,UINT stride,UINT x,UINT y){BASIC_DISPLAY_DRIVER *d=from_adapter(h);if(d)d->SystemDisplayWrite(source,w,height,stride,x,y);}
static NTSTATUS brightness_interface(void *h,PQUERY_INTERFACE q){BASIC_DISPLAY_DRIVER *d=from_adapter((HANDLE)h);return d?d->QueryBrightness(q):STATUS_DEVICE_NOT_READY;}
extern "C" NTSTATUS GeoDisplayFillVidPnCallbacks(DRIVER_INITIALIZATION_DATA *t) {
 if(!t)return STATUS_INVALID_PARAMETER;
 t->DxgkDdiQueryInterface=brightness_interface;
 t->DxgkDdiIsSupportedVidPn=supported;t->DxgkDdiRecommendFunctionalVidPn=recommend;
 t->DxgkDdiEnumVidPnCofuncModality=enum_modes;t->DxgkDdiSetVidPnSourceVisibility=visibility;
 t->DxgkDdiCommitVidPn=commit;t->DxgkDdiUpdateActiveVidPnPresentPath=update;
 t->DxgkDdiRecommendMonitorModes=monitor;t->DxgkDdiQueryVidPnHWCapability=hwcap;
 t->DxgkDdiSetPalette=palette;
 t->DxgkDdiSetPointerPosition=pointer_pos;t->DxgkDdiSetPointerShape=pointer_shape;
 t->DxgkDdiQueryChildRelations=child_relations;t->DxgkDdiQueryChildStatus=child_status;
 t->DxgkDdiQueryDeviceDescriptor=descriptor;t->DxgkDdiDispatchIoRequest=dispatch;
 t->DxgkDdiStopDeviceAndReleasePostDisplayOwnership=release_post;
 t->DxgkDdiResetDevice=reset_display;t->DxgkDdiSystemDisplayEnable=system_enable;t->DxgkDdiSystemDisplayWrite=system_write;
 return STATUS_SUCCESS;
}

extern "C" NTSTATUS GeoDisplaySetBrightnessResources(void *handle,PCM_RESOURCE_LIST resources){
 geo_display *s=state(handle);if(!s||s->started||!resources)return STATUS_INVALID_PARAMETER;
 s->bdd->SetUnifiedBrightnessResources(resources);return STATUS_SUCCESS;
}

extern "C" NTSTATUS GeoDisplaySetPanelTiming(void *handle,const struct a04_panel_timing *timing){
 geo_display *s=state(handle);struct a04_panel_frequencies f;
 if(!s||s->started||KeGetCurrentIrql()!=PASSIVE_LEVEL||!a04_panel_timing_frequencies(timing,&f))return STATUS_INVALID_PARAMETER;
 s->bdd->SetPanelTiming(timing);return STATUS_SUCCESS;
}
