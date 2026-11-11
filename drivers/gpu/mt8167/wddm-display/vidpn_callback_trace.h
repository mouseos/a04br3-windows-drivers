/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded optional evidence, same PASSIVE/APC boundary as StartGap359.
 * No pointers, private payload, flush, IRQ or hardware operations.
 */
#ifndef GEO_VIDPN_CALLBACK_TRACE_H
#define GEO_VIDPN_CALLBACK_TRACE_H
#define GEO_VIDPN_TRACE_KINDS 7u
#define GEO_VIDPN_TRACE_SLOTS 4u
struct geo_vidpn_trace_state {
 ULONG sequence;ULONGLONG epoch_filetime,epoch_interrupt;
 volatile LONG claims[GEO_VIDPN_TRACE_KINDS],failure[GEO_VIDPN_TRACE_KINDS],unsupported;
};
struct geo_vidpn_trace_record {
 ULONG schema,bytes,kind,slot,complete,status,sequence,eligible;
 ULONGLONG epoch_filetime,epoch_interrupt,begin_filetime,begin_interrupt,end_filetime,end_interrupt;
 ULONG data[88];
};
C_ASSERT(sizeof(geo_vidpn_trace_record)==432);
static volatile LONG geo_vidpn_start_sequence;
static BOOLEAN GeoVidPnTraceSafe(void){return KeGetCurrentIrql()==PASSIVE_LEVEL&&!KeAreAllApcsDisabled();}
static void GeoVidPnTraceStore(const geo_vidpn_trace_record *r,ULONG category=0){
 UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\VidPnCallbackTraceV1");
 UNICODE_STRING name;OBJECT_ATTRIBUTES oa;HANDLE key=NULL;ULONG disposition;WCHAR label[5]={L'C',0,L'S',0,0};
 if(!GeoVidPnTraceSafe())return;
 if(!r->kind)RtlInitUnicodeString(&name,L"EpochV1");
 else {label[1]=(WCHAR)(L'0'+r->kind);label[2]=category==1?L'F':category==2?L'L':category==3?L'U':L'S';label[3]=(WCHAR)(L'0'+(category?0:r->slot));RtlInitUnicodeString(&name,label);}
 InitializeObjectAttributes(&oa,&path,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 if(NT_SUCCESS(ZwCreateKey(&key,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition))){
  (void)ZwSetValueKey(key,&name,0,REG_BINARY,(PVOID)r,sizeof(*r));ZwClose(key);
 }
}
static void GeoVidPnTraceEpoch(geo_vidpn_trace_state *s,NTSTATUS status,BOOLEAN complete){
 geo_vidpn_trace_record r={};LARGE_INTEGER now;
 if(!GeoVidPnTraceSafe())return;
 if(!complete){RtlZeroMemory(s,sizeof(*s));s->sequence=(ULONG)InterlockedIncrement(&geo_vidpn_start_sequence);
  KeQuerySystemTime(&now);s->epoch_filetime=(ULONGLONG)now.QuadPart;s->epoch_interrupt=KeQueryInterruptTime();}
 r.schema=1;r.bytes=sizeof(r);r.complete=complete;r.status=(ULONG)status;r.sequence=s->sequence;r.eligible=1;
 r.epoch_filetime=s->epoch_filetime;r.epoch_interrupt=s->epoch_interrupt;
 KeQuerySystemTime(&now);r.end_filetime=(ULONGLONG)now.QuadPart;r.end_interrupt=KeQueryInterruptTime();
 r.data[0]=GEO_VIDPN_TRACE_KINDS;r.data[1]=GEO_VIDPN_TRACE_SLOTS;r.data[2]=0; /* No call cutoff for the single latest slot. */GeoVidPnTraceStore(&r);
}
static void GeoVidPnTraceBegin(geo_vidpn_trace_state *s,ULONG kind,geo_vidpn_trace_record *r){
 LARGE_INTEGER now;LONG n,prior;RtlZeroMemory(r,sizeof(*r));r->slot=MAXULONG;
 if(!s||!s->sequence||!kind||kind>GEO_VIDPN_TRACE_KINDS||!GeoVidPnTraceSafe())return;
 r->schema=1;r->bytes=sizeof(*r);r->kind=kind;r->status=(ULONG)STATUS_PENDING;r->sequence=s->sequence;r->eligible=1;
 r->epoch_filetime=s->epoch_filetime;r->epoch_interrupt=s->epoch_interrupt;
 KeQuerySystemTime(&now);r->begin_filetime=(ULONGLONG)now.QuadPart;r->begin_interrupt=KeQueryInterruptTime();
 n=s->claims[kind-1];while(n<MAXLONG){prior=InterlockedCompareExchange(&s->claims[kind-1],n+1,n);if(prior==n)break;n=prior;}r->slot=(ULONG)n;r->data[29]=n==MAXLONG;
}
static void GeoVidPnTraceEntry(const geo_vidpn_trace_record *r){if(r->eligible){if(r->slot<GEO_VIDPN_TRACE_SLOTS)GeoVidPnTraceStore(r);GeoVidPnTraceStore(r,2);}}
static NTSTATUS GeoVidPnTraceEnd(geo_vidpn_trace_state *s,geo_vidpn_trace_record *r,NTSTATUS status){
 LARGE_INTEGER now;
 if(r->eligible&&GeoVidPnTraceSafe()){
  r->complete=1;r->status=(ULONG)status;KeQuerySystemTime(&now);r->end_filetime=(ULONGLONG)now.QuadPart;r->end_interrupt=KeQueryInterruptTime();
  if(r->slot<GEO_VIDPN_TRACE_SLOTS)GeoVidPnTraceStore(r);
  GeoVidPnTraceStore(r,2);
  if(!NT_SUCCESS(status)&&InterlockedCompareExchange(&s->failure[r->kind-1],1,0)==0)GeoVidPnTraceStore(r,1);
  if(r->kind==1&&NT_SUCCESS(status)&&r->data[31]==0&&InterlockedCompareExchange(&s->unsupported,1,0)==0)GeoVidPnTraceStore(r,3);
 }
 return status;
}
/* Unlocked numeric observations, not an atomic display-state snapshot. */
static void GeoVidPnTraceMode(geo_vidpn_trace_record *r,BASIC_DISPLAY_DRIVER *d,ULONG at){
 if(!r->eligible||!d||!GeoVidPnTraceSafe())return;
 const CURRENT_BDD_MODE *m=d->GetCurrentMode(0);if(!m)return;
 ULONG *v=r->data+at;v[0]=m->DispInfo.Width;v[1]=m->DispInfo.Height;v[2]=m->DispInfo.Pitch;v[3]=(ULONG)m->DispInfo.ColorFormat;
 v[4]=m->SrcModeWidth;v[5]=m->SrcModeHeight;v[6]=(ULONG)m->Rotation;v[7]=(ULONG)m->Scaling;
 v[8]=m->Flags.FrameBufferIsActive;v[9]=m->Flags.SourceNotVisible;v[10]=1;
}
static void GeoVidPnTraceSignal(ULONG *v,const D3DKMDT_VIDEO_SIGNAL_INFO *s){
 v[9]=s->ActiveSize.cx;v[10]=s->ActiveSize.cy;v[11]=s->TotalSize.cx;v[12]=s->TotalSize.cy;
 v[13]=s->VSyncFreq.Numerator;v[14]=s->VSyncFreq.Denominator;v[15]=s->HSyncFreq.Numerator;v[16]=s->HSyncFreq.Denominator;
 v[17]=(ULONG)s->PixelRate;v[18]=(ULONG)(s->PixelRate>>32);v[19]=(ULONG)s->ScanLineOrdering;
}
/* Read-only OS mode-set references. Every acquired reference is released;
 * query/cleanup errors are evidence only and NEVER replace the DDI result. */
static void GeoVidPnTraceModes(geo_vidpn_trace_record *r,BASIC_DISPLAY_DRIVER *d,D3DKMDT_HVIDPN vidpn,ULONG at){
 if(!r->eligible||!d||!vidpn||!GeoVidPnTraceSafe())return;
 ULONG *v=r->data+at;for(ULONG i=0;i<28;i++)v[i]=MAXULONG;r->data[30]|=at==32?1u:2u;
 const DXGK_VIDPN_INTERFACE *api=NULL;const DXGK_VIDPNSOURCEMODESET_INTERFACE *src=NULL;
 const DXGK_VIDPNTARGETMODESET_INTERFACE *dst=NULL;D3DKMDT_HVIDPNSOURCEMODESET hs=0;D3DKMDT_HVIDPNTARGETMODESET ht=0;
 const D3DKMDT_VIDPN_SOURCE_MODE *sm=NULL;const D3DKMDT_VIDPN_TARGET_MODE *tm=NULL;SIZE_T count=0;
 v[0]=(ULONG)d->GetDxgkInterface()->DxgkCbQueryVidPnInterface(vidpn,DXGK_VIDPN_INTERFACE_VERSION_V1,&api);
 if(!NT_SUCCESS((NTSTATUS)v[0])||!api)return;
 v[1]=(ULONG)api->pfnAcquireSourceModeSet(vidpn,0,&hs,&src);
 if(NT_SUCCESS((NTSTATUS)v[1])&&src){
  v[26]=(ULONG)src->pfnGetNumModes(hs,&count);if(NT_SUCCESS((NTSTATUS)v[26]))v[24]=(ULONG)count;
  v[2]=(ULONG)src->pfnAcquirePinnedModeInfo(hs,&sm);v[20]=NT_SUCCESS((NTSTATUS)v[2])&&sm?1u:0u;
  if(NT_SUCCESS((NTSTATUS)v[2])&&sm&&sm->Type==D3DKMDT_RMT_GRAPHICS){v[3]=sm->Format.Graphics.PrimSurfSize.cx;v[4]=sm->Format.Graphics.PrimSurfSize.cy;v[5]=sm->Format.Graphics.Stride;v[6]=(ULONG)sm->Format.Graphics.PixelFormat;}
  if(NT_SUCCESS((NTSTATUS)v[2])&&sm)(void)src->pfnReleaseModeInfo(hs,sm);
 }
 if(NT_SUCCESS((NTSTATUS)v[1])&&hs)v[22]=(ULONG)api->pfnReleaseSourceModeSet(vidpn,hs);
 v[7]=(ULONG)api->pfnAcquireTargetModeSet(vidpn,0,&ht,&dst);
 if(NT_SUCCESS((NTSTATUS)v[7])&&dst){
  count=0;v[27]=(ULONG)dst->pfnGetNumModes(ht,&count);if(NT_SUCCESS((NTSTATUS)v[27]))v[25]=(ULONG)count;
  v[8]=(ULONG)dst->pfnAcquirePinnedModeInfo(ht,&tm);v[21]=NT_SUCCESS((NTSTATUS)v[8])&&tm?1u:0u;
  if(NT_SUCCESS((NTSTATUS)v[8])&&tm)GeoVidPnTraceSignal(v,&tm->VideoSignalInfo);
  if(NT_SUCCESS((NTSTATUS)v[8])&&tm)(void)dst->pfnReleaseModeInfo(ht,tm);
 }
 if(NT_SUCCESS((NTSTATUS)v[7])&&ht)v[23]=(ULONG)api->pfnReleaseTargetModeSet(vidpn,ht);
}
static void GeoVidPnTraceMonitor(geo_vidpn_trace_record *r,const DXGKARG_RECOMMENDMONITORMODES *p,ULONG at){
 if(!r->eligible||!p||!p->pMonitorSourceModeSetInterface||!GeoVidPnTraceSafe())return;
 ULONG *v=r->data+at;for(ULONG i=0;i<28;i++)v[i]=MAXULONG;r->data[30]|=at==32?4u:8u;
 const DXGK_MONITORSOURCEMODESET_INTERFACE *api=p->pMonitorSourceModeSetInterface;
 const D3DKMDT_MONITOR_SOURCE_MODE *m=NULL;SIZE_T count=0;
 v[27]=(ULONG)api->pfnGetNumModes(p->hMonitorSourceModeSet,&count);if(NT_SUCCESS((NTSTATUS)v[27]))v[25]=(ULONG)count;
 v[8]=(ULONG)api->pfnAcquireFirstModeInfo(p->hMonitorSourceModeSet,&m);v[21]=NT_SUCCESS((NTSTATUS)v[8])&&m?1u:0u;
 if(NT_SUCCESS((NTSTATUS)v[8])&&m)GeoVidPnTraceSignal(v,&m->VideoSignalInfo);
 if(NT_SUCCESS((NTSTATUS)v[8])&&m)v[23]=(ULONG)api->pfnReleaseModeInfo(p->hMonitorSourceModeSet,m);
}
#endif
