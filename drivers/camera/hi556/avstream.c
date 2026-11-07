/* SPDX-License-Identifier: GPL-2.0-only
 * Windows8.1 AVStream pin over the sole board-qualified HI556 RAW owner.
 * No MMIO, I2C, power or DMA ownership here. No synthetic source/fallback.
 * Contract reference: Microsoft avstream/avssamp + WDK8.1 ks.h/ksmedia.h. */
#define CINTERFACE
#define COBJMACROS
#include <ntddk.h>
#include <windef.h>
#define NOBITMAP
#include <mmreg.h>
#undef NOBITMAP
#include <unknown.h>
#include <ks.h>
#include <ksmedia.h>
#include "capture_wire.h"
#ifndef A04_HI556_AVSTREAM_EXPERIMENT
#define A04_HI556_AVSTREAM_EXPERIMENT 0
#endif
#define TAG 'v655'
#define RAW_BYTES (sizeof(A04_CAM_STATUS)+A04_HI556_FRAME_BYTES)
#define INTERVAL UINT64_C(10000000) /* software maximum1fps, not a HW fps claim */
static const GUID raw_guid=A04_HI556_CAPTURE_GUID_VALUES;
static const GUID video={STATICGUIDOF(KSDATAFORMAT_TYPE_VIDEO)};
static const GUID rgb24={STATICGUIDOF(KSDATAFORMAT_SUBTYPE_RGB24)};
static const GUID vi={STATICGUIDOF(KSDATAFORMAT_SPECIFIER_VIDEOINFO)};
static const GUID capture={STATICGUIDOF(PINNAME_VIDEO_CAPTURE)};
static const GUID filter_guid={0x50655601,0x2b19,0x4637,{0x8c,0x2e,0x5e,0x56,0xd0,0x03,0x00,0x01}};
static const GUID categories[]={
 {STATICGUIDOF(KSCATEGORY_VIDEO)}, {STATICGUIDOF(KSCATEGORY_CAPTURE)},
 {STATICGUIDOF(KSCATEGORY_VIDEO_CAMERA)}
};
static int same(const GUID *a,const GUID *b){return RtlCompareMemory(a,b,sizeof(*a))==sizeof(*a);}
struct pin_context {
 KMUTEX lock;HANDLE raw;UCHAR *buffer;PIKSREFERENCECLOCK clock;
 KSSTATE state;volatile LONG stopping;NTSTATUS failed;
 uint64_t next_due;LONGLONG picture,drops;
};
static NTSTATUS lower_ioctl(HANDLE h,ULONG code,void *out,ULONG bytes,ULONG_PTR *used)
{
 IO_STATUS_BLOCK ios={0};NTSTATUS s;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 s=ZwDeviceIoControlFile(h,NULL,NULL,NULL,&ios,code,NULL,0,out,bytes);
 if(s==STATUS_PENDING){s=ZwWaitForSingleObject(h,FALSE,NULL);if(NT_SUCCESS(s))s=ios.Status;}
 if(NT_SUCCESS(s))*used=ios.Information;else *used=0;
 return s;
}
static NTSTATUS open_raw(HANDLE *out)
{
 PWSTR list=NULL;UNICODE_STRING name;OBJECT_ATTRIBUTES oa;IO_STATUS_BLOCK ios;
 NTSTATUS s=IoGetDeviceInterfaces(&raw_guid,NULL,0,&list);*out=NULL;
 if(!NT_SUCCESS(s))return s;
 /* Exactly one enabled, board-qualified rear owner; never pick arbitrarily. */
 if(!list[0]){ExFreePool(list);return STATUS_NO_SUCH_DEVICE;}
 RtlInitUnicodeString(&name,list);
 if(list[name.Length/sizeof(WCHAR)+1]){ExFreePool(list);return STATUS_OBJECT_NAME_COLLISION;}
 InitializeObjectAttributes(&oa,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 s=ZwCreateFile(out,FILE_READ_DATA|SYNCHRONIZE,&oa,&ios,NULL,0,
  FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN,FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE,NULL,0);
 ExFreePool(list);
 if(NT_SUCCESS(s)){
  A04_CAM_STATUS status;ULONG_PTR used=0;RtlZeroMemory(&status,sizeof(status));
  s=lower_ioctl(*out,A04_CAM_IOCTL_STATUS,&status,sizeof(status),&used);
  if(NT_SUCCESS(s)&&(used!=sizeof(status)||status.version!=A04_CAM_STATUS_VERSION||status.bytes!=sizeof(status)||
    !status.identity.id_observed||status.identity.chip_id!=0x0556||status.identity.quarantined||status.power_lost))s=STATUS_DEVICE_NOT_READY;
  if(!NT_SUCCESS(s)){ZwClose(*out);*out=NULL;}
 }
 return s;
}
static NTSTATUS device_start(PKSDEVICE device,PIRP irp,PCM_RESOURCE_LIST translated,PCM_RESOURCE_LIST raw)
{
 HANDLE h=NULL;NTSTATUS s;UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(irp);
 UNREFERENCED_PARAMETER(translated);UNREFERENCED_PARAMETER(raw);
 if(!A04_HI556_AVSTREAM_EXPERIMENT)return STATUS_NOT_SUPPORTED;
 s=open_raw(&h);if(h)ZwClose(h);return s;
}
static NTSTATUS valid_format(PKSDATAFORMAT f)
{
 PKS_DATAFORMAT_VIDEOINFOHEADER v=(PKS_DATAFORMAT_VIDEOINFOHEADER)f;
 if(!f||f->FormatSize!=sizeof(*v)||f->Flags||f->Reserved||f->SampleSize!=A04_HI556_RGB_BYTES||
  !same(&f->MajorFormat,&video)||!same(&f->SubFormat,&rgb24)||!same(&f->Specifier,&vi))return STATUS_NO_MATCH;
 if(v->VideoInfoHeader.bmiHeader.biSize!=sizeof(KS_BITMAPINFOHEADER)||
  v->VideoInfoHeader.bmiHeader.biWidth!=A04_HI556_RGB_WIDTH||v->VideoInfoHeader.bmiHeader.biHeight!=A04_HI556_RGB_HEIGHT||
  v->VideoInfoHeader.bmiHeader.biPlanes!=1||v->VideoInfoHeader.bmiHeader.biBitCount!=24||
  v->VideoInfoHeader.bmiHeader.biCompression!=KS_BI_RGB||v->VideoInfoHeader.bmiHeader.biSizeImage!=A04_HI556_RGB_BYTES||
  v->VideoInfoHeader.AvgTimePerFrame!=INTERVAL||v->VideoInfoHeader.bmiHeader.biClrUsed||v->VideoInfoHeader.bmiHeader.biClrImportant)
  return STATUS_NO_MATCH;
 /* This first format has no cropping/scaling contract. */
 {RECT z={0};RECT full={0,0,A04_HI556_RGB_WIDTH,A04_HI556_RGB_HEIGHT};
  if((RtlCompareMemory(&v->VideoInfoHeader.rcSource,&z,sizeof(z))!=sizeof(z)&&RtlCompareMemory(&v->VideoInfoHeader.rcSource,&full,sizeof(z))!=sizeof(z))||
     (RtlCompareMemory(&v->VideoInfoHeader.rcTarget,&z,sizeof(z))!=sizeof(z)&&RtlCompareMemory(&v->VideoInfoHeader.rcTarget,&full,sizeof(z))!=sizeof(z)))return STATUS_NO_MATCH;}
 return STATUS_SUCCESS;
}
static NTSTATUS pin_create(PKSPIN pin,PIRP irp)
{
 struct pin_context *c;NTSTATUS s=valid_format(pin->ConnectionFormat);UNREFERENCED_PARAMETER(irp);
 if(!NT_SUCCESS(s))return s;
 c=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*c),TAG);if(!c)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(c,sizeof(*c));KeInitializeMutex(&c->lock,0);
 c->buffer=ExAllocatePoolWithTag(NonPagedPoolNx,RAW_BYTES,TAG);
 if(!c->buffer){ExFreePoolWithTag(c,TAG);return STATUS_INSUFFICIENT_RESOURCES;}
 c->state=KSSTATE_STOP;c->stopping=1;pin->Context=c;
 pin->StreamHeaderSize=sizeof(KSSTREAM_HEADER)+sizeof(KS_FRAME_INFO);return STATUS_SUCCESS;
}
static void close_lower(struct pin_context *c)
{
 if(c->clock){c->clock->lpVtbl->Release(c->clock);c->clock=NULL;}
 if(c->raw){ZwClose(c->raw);c->raw=NULL;}
}
static NTSTATUS pin_close(PKSPIN pin,PIRP irp)
{
 struct pin_context *c=pin->Context;UNREFERENCED_PARAMETER(irp);
 if(!c)return STATUS_SUCCESS;
 InterlockedExchange(&c->stopping,1);
 /* No queued worker/DPC and no outstanding request after the synchronous
  * Process returns. AVStream holds the pin lifetime; serialize lower use. */
 KeWaitForSingleObject(&c->lock,Executive,KernelMode,FALSE,NULL);
 close_lower(c);RtlSecureZeroMemory(c->buffer,RAW_BYTES);ExFreePoolWithTag(c->buffer,TAG);
 pin->Context=NULL;KeReleaseMutex(&c->lock,FALSE);ExFreePoolWithTag(c,TAG);return STATUS_SUCCESS;
}
static NTSTATUS set_state(PKSPIN pin,KSSTATE to,KSSTATE from)
{
 struct pin_context *c=pin->Context;NTSTATUS s=STATUS_SUCCESS;UNREFERENCED_PARAMETER(from);
 if(!c)return STATUS_INVALID_DEVICE_STATE;
 if(to!=KSSTATE_RUN)InterlockedExchange(&c->stopping,1);
 KeWaitForSingleObject(&c->lock,Executive,KernelMode,FALSE,NULL);
 if(to==KSSTATE_STOP){close_lower(c);c->failed=0;c->picture=c->drops=0;c->next_due=0;}
 else if(!c->raw){s=open_raw(&c->raw);
  if(NT_SUCCESS(s)&&!NT_SUCCESS(KsPinGetReferenceClockInterface(pin,&c->clock)))c->clock=NULL;}
 if(NT_SUCCESS(s)){c->state=to;if(to==KSSTATE_RUN)InterlockedExchange(&c->stopping,0);}
 KeReleaseMutex(&c->lock,FALSE);return s;
}
static NTSTATUS set_format(PKSPIN pin,PKSDATAFORMAT old,PKSMULTIPLE_ITEM attrs,const KSDATARANGE *range,const KSATTRIBUTE_LIST *ar)
{
 UNREFERENCED_PARAMETER(attrs);UNREFERENCED_PARAMETER(range);UNREFERENCED_PARAMETER(ar);
 if(old)return STATUS_INVALID_DEVICE_STATE; /* fixed format for pin lifetime */
 return valid_format(pin->ConnectionFormat);
}
static NTSTATUS process(PKSPIN pin)
{
 struct pin_context *c=pin->Context;PKSSTREAM_POINTER p;PIRP irp;NTSTATUS s=STATUS_SUCCESS;
 ULONG_PTR bytes=0;A04_CAM_STATUS *raw;uint64_t now;
 if(!c||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 p=KsPinGetLeadingEdgeStreamPointer(pin,KSSTREAM_POINTER_STATE_LOCKED);if(!p)return STATUS_PENDING;
 irp=KsStreamPointerGetIrp(p);
 KeWaitForSingleObject(&c->lock,Executive,KernelMode,FALSE,NULL);
 if(c->state!=KSSTATE_RUN||c->stopping||!c->raw){s=STATUS_CANCELLED;goto done;}
 if(!NT_SUCCESS(c->failed)){s=c->failed;goto done;}
 if(!p->StreamHeader->Data||!p->OffsetOut.Data||p->OffsetOut.Remaining<A04_HI556_RGB_BYTES){s=STATUS_BUFFER_TOO_SMALL;goto done;}
 /* Never sleep or access registry/MMIO at dispatch level. Pace actual
  * requests at <=1fps; acquisition may run slower and has no fake cadence. */
 for(now=KeQueryInterruptTime();now<c->next_due;now=KeQueryInterruptTime()){
  LARGE_INTEGER wait;wait.QuadPart=-500000;
  if(c->stopping||(irp&&irp->Cancel)){s=STATUS_CANCELLED;goto done;}
  s=KeDelayExecutionThread(KernelMode,FALSE,&wait);if(!NT_SUCCESS(s))goto done;
 }
 if(irp&&irp->Cancel){s=STATUS_CANCELLED;goto done;}
 RtlZeroMemory(c->buffer,RAW_BYTES);
 s=lower_ioctl(c->raw,A04_CAM_IOCTL_CAPTURE,c->buffer,RAW_BYTES,&bytes);
 raw=(A04_CAM_STATUS *)c->buffer;
 now=KeQueryInterruptTime();c->next_due=now+INTERVAL;
 if(!NT_SUCCESS(s)){c->failed=s;goto done;}
 if(bytes!=RAW_BYTES||raw->version!=A04_CAM_STATUS_VERSION||raw->bytes!=sizeof(*raw)||raw->status!=STATUS_SUCCESS||
    raw->frame_bytes!=A04_HI556_FRAME_BYTES||raw->width!=A04_HI556_WIDTH||raw->height!=A04_HI556_HEIGHT||
    raw->stride!=A04_HI556_STRIDE||raw->format!=2||!raw->receiver.pass1||!raw->receiver.imgo||
    !raw->receiver.quiesced||raw->receiver.uncertain||raw->receiver.guard_errors||raw->power_lost||
    !raw->stop_observed||!NT_SUCCESS(raw->stop_status)||!raw->content.nonzero||!raw->content.nonconstant){
  s=c->failed=STATUS_DATA_ERROR;goto done;
 }
 if(c->stopping||(irp&&irp->Cancel)){s=STATUS_CANCELLED;goto done;}
 if(a04_hi556_rgb24(c->buffer+sizeof(*raw),A04_HI556_FRAME_BYTES,p->OffsetOut.Data,A04_HI556_RGB_BYTES)){
  s=c->failed=STATUS_DATA_ERROR;goto done;
 }
 p->StreamHeader->DataUsed=A04_HI556_RGB_BYTES;p->StreamHeader->Duration=0;
 p->StreamHeader->OptionsFlags=KSSTREAM_HEADER_OPTIONSF_SPLICEPOINT;
 /* Real clock sample at completed acquisition. Never tag interrupt-time as
  * an unrelated KS clock epoch or assert a measured frame duration. */
 if(c->clock){p->StreamHeader->PresentationTime.Time=c->clock->lpVtbl->GetTime(c->clock);
  p->StreamHeader->PresentationTime.Numerator=p->StreamHeader->PresentationTime.Denominator=1;
  p->StreamHeader->OptionsFlags|=KSSTREAM_HEADER_OPTIONSF_TIMEVALID;}
 if(c->drops)p->StreamHeader->OptionsFlags|=KSSTREAM_HEADER_OPTIONSF_DATADISCONTINUITY;
 c->picture++;
 if(p->StreamHeader->Size>=sizeof(KSSTREAM_HEADER)+sizeof(KS_FRAME_INFO)){
  PKS_FRAME_INFO f=(PKS_FRAME_INFO)(p->StreamHeader+1);RtlZeroMemory(f,sizeof(*f));
  f->ExtendedHeaderSize=sizeof(*f);f->PictureNumber=c->picture;f->DropCount=c->drops;
 }
done:
 if(!NT_SUCCESS(s)){p->StreamHeader->DataUsed=0;c->drops++;KsStreamPointerSetStatusCode(p,s);}
 RtlSecureZeroMemory(c->buffer,RAW_BYTES);
 KeReleaseMutex(&c->lock,FALSE);
 if(NT_SUCCESS(s))KsStreamPointerAdvanceOffsetsAndUnlock(p,0,A04_HI556_RGB_BYTES,TRUE);
 else KsStreamPointerUnlock(p,TRUE);
 return STATUS_SUCCESS; /* exact frame error lives on its KS IRP */
}
static const KS_DATARANGE_VIDEO range={
 {sizeof(KS_DATARANGE_VIDEO),0,A04_HI556_RGB_BYTES,0,
 {STATICGUIDOF(KSDATAFORMAT_TYPE_VIDEO)}, {STATICGUIDOF(KSDATAFORMAT_SUBTYPE_RGB24)},
 {STATICGUIDOF(KSDATAFORMAT_SPECIFIER_VIDEOINFO)}},
 TRUE,FALSE,0,0,
 {{STATICGUIDOF(KSDATAFORMAT_SPECIFIER_VIDEOINFO)},0,
 {A04_HI556_RGB_WIDTH,A04_HI556_RGB_HEIGHT},
 {A04_HI556_RGB_WIDTH,A04_HI556_RGB_HEIGHT},{A04_HI556_RGB_WIDTH,A04_HI556_RGB_HEIGHT},1,1,1,1,
 {A04_HI556_RGB_WIDTH,A04_HI556_RGB_HEIGHT},{A04_HI556_RGB_WIDTH,A04_HI556_RGB_HEIGHT},1,1,0,0,0,0,
 INTERVAL,INTERVAL,A04_HI556_RGB_BYTES*8,A04_HI556_RGB_BYTES*8},
 {{0,0,0,0},{0,0,0,0},A04_HI556_RGB_BYTES*8,0,INTERVAL,
 {sizeof(KS_BITMAPINFOHEADER),A04_HI556_RGB_WIDTH,A04_HI556_RGB_HEIGHT,1,24,KS_BI_RGB,A04_HI556_RGB_BYTES,0,0,0,0}}
};
static NTSTATUS intersect(PVOID context,PIRP irp,PKSP_PIN pin,PKSDATARANGE caller,PKSDATARANGE matching,ULONG size,PVOID out,PULONG needed)
{
 PKS_DATAFORMAT_VIDEOINFOHEADER f=out;const KS_DATARANGE_VIDEO *v=(const KS_DATARANGE_VIDEO *)caller;
 UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(irp);UNREFERENCED_PARAMETER(pin);UNREFERENCED_PARAMETER(matching);
 *needed=0;
 if(caller->FormatSize<sizeof(*v)||caller->Flags||!same(&caller->MajorFormat,&video)||!same(&caller->SubFormat,&rgb24)||!same(&caller->Specifier,&vi)||
  !v->bFixedSizeSamples||v->bTemporalCompression||v->StreamDescriptionFlags||v->MemoryAllocationFlags||
  v->ConfigCaps.MinOutputSize.cx>A04_HI556_RGB_WIDTH||v->ConfigCaps.MaxOutputSize.cx<A04_HI556_RGB_WIDTH||
  v->ConfigCaps.MinOutputSize.cy>A04_HI556_RGB_HEIGHT||v->ConfigCaps.MaxOutputSize.cy<A04_HI556_RGB_HEIGHT||
  v->ConfigCaps.MinFrameInterval>INTERVAL||v->ConfigCaps.MaxFrameInterval<INTERVAL)return STATUS_NO_MATCH;
 *needed=sizeof(*f);if(!size)return STATUS_BUFFER_OVERFLOW;
 if(size<sizeof(*f))return STATUS_BUFFER_TOO_SMALL;
 if(!out)return STATUS_INVALID_PARAMETER;
 f->DataFormat=range.DataRange;f->DataFormat.FormatSize=sizeof(*f);f->VideoInfoHeader=range.VideoInfoHeader;return STATUS_SUCCESS;
}
static const KSPIN_DISPATCH pin_dispatch={pin_create,pin_close,process,NULL,set_format,set_state,NULL,NULL,NULL,NULL};
DECLARE_SIMPLE_FRAMING_EX(framing,STATICGUIDOF(KSMEMORY_TYPE_KERNEL_NONPAGED),
 KSALLOCATOR_REQUIREMENTF_SYSTEM_MEMORY|KSALLOCATOR_REQUIREMENTF_PREFERENCES_ONLY,2,0,A04_HI556_RGB_BYTES,A04_HI556_RGB_BYTES);
static const PKSDATARANGE ranges[]={(PKSDATARANGE)&range};
static const KSPIN_DESCRIPTOR_EX pins[]={
 {&pin_dispatch,NULL,{0,NULL,0,NULL,1,ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_BOTH,&capture,&capture,0},
 KSPIN_FLAG_PROCESS_IN_RUN_STATE_ONLY|KSPIN_FLAG_FIXED_FORMAT,1,0,&framing,intersect}
};
static const KSFILTER_DESCRIPTOR filter={NULL,NULL,KSFILTER_DESCRIPTOR_VERSION,0,&filter_guid,
 1,sizeof(pins[0]),pins,3,categories,0,sizeof(KSNODE_DESCRIPTOR),NULL,0,NULL,NULL};
static const KSFILTER_DESCRIPTOR *const filters[]={&filter};
static const KSDEVICE_DISPATCH device_dispatch={NULL,device_start};
static const KSDEVICE_DESCRIPTOR descriptor={&device_dispatch,1,filters,KSDEVICE_DESCRIPTOR_VERSION};
DRIVER_INITIALIZE DriverEntry;
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{
 if(!A04_HI556_AVSTREAM_EXPERIMENT)return STATUS_NOT_SUPPORTED;
 return KsInitializeDriver(object,path,&descriptor);
}
