/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Independent narrowband KS WaveRT speaker/mic miniports. No tone, position
 * timer or shadow DMA. All buffers/position/latency come from the real owner.
 */
#include <initguid.h>
#include "ks_endpoints.h"
#define TAG 'k36M'
struct BtPlacement {};
inline void *operator new(size_t,void *p,BtPlacement){return p;}
static bool format_ok(PKSDATAFORMAT f)
{
 if(!f||f->FormatSize<sizeof(KSDATAFORMAT_WAVEFORMATEX)||
    !IsEqualGUIDAligned(f->MajorFormat,KSDATAFORMAT_TYPE_AUDIO)||
    !IsEqualGUIDAligned(f->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)||
    !IsEqualGUIDAligned(f->Specifier,KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))return false;
 WAVEFORMATEX *w=&((PKSDATAFORMAT_WAVEFORMATEX)f)->WaveFormatEx;
 if(w->nChannels!=1||w->nSamplesPerSec!=8000||w->wBitsPerSample!=16||w->nBlockAlign!=2||w->nAvgBytesPerSec!=16000)return false;
 if(w->wFormatTag==WAVE_FORMAT_PCM)return w->cbSize==0;
 if(w->wFormatTag!=WAVE_FORMAT_EXTENSIBLE||f->FormatSize<sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE)||w->cbSize<22)return false;
 WAVEFORMATEXTENSIBLE *e=(WAVEFORMATEXTENSIBLE *)w;
 return e->Samples.wValidBitsPerSample==16&&e->dwChannelMask==KSAUDIO_SPEAKER_MONO&&IsEqualGUIDAligned(e->SubFormat,KSDATAFORMAT_SUBTYPE_PCM);
}
static KSDATARANGE_AUDIO range={{sizeof(KSDATARANGE_AUDIO),0,0,0,STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)},1,16,16,8000,8000};
static KSDATARANGE bridge={sizeof(KSDATARANGE),0,0,0,STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)};
static PKSDATARANGE ranges[]={reinterpret_cast<PKSDATARANGE>(&range)},bridges[]={&bridge};
static PCPIN_DESCRIPTOR renderPins[]={
 {1,1,0,NULL,{0,NULL,0,NULL,1,ranges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,NULL,0}},
 {0,0,0,NULL,{0,NULL,0,NULL,1,bridges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}};
static PCPIN_DESCRIPTOR capturePins[]={
 {1,1,0,NULL,{0,NULL,0,NULL,1,ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,NULL,0}},
 {0,0,0,NULL,{0,NULL,0,NULL,1,bridges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}};
static PCCONNECTION_DESCRIPTOR renderConnections[]={{PCFILTER_NODE,0,PCFILTER_NODE,1}},captureConnections[]={{PCFILTER_NODE,1,PCFILTER_NODE,0}};
static PCFILTER_DESCRIPTOR filters[]={
 {0,NULL,sizeof(PCPIN_DESCRIPTOR),2,renderPins,sizeof(PCNODE_DESCRIPTOR),0,NULL,1,renderConnections,0,NULL},
 {0,NULL,sizeof(PCPIN_DESCRIPTOR),2,capturePins,sizeof(PCNODE_DESCRIPTOR),0,NULL,1,captureConnections,0,NULL}};
class HfpStream : public IMiniportWaveRTStream {
 volatile LONG refs,retiring,deferred;KSPIN_LOCK bufferLock;BOOLEAN readable;
 struct a04_bt_hfp_session *session;ULONG direction;PPORTWAVERTSTREAM port;
 struct a04_bt_hfp_buffer buffer;struct a04_bt_hfp_latency latency;KSSTATE state;
 const struct a04_bt_hfp_provider *p() {return a04_bt_hfp_provider(session);}
 NTSTATUS free_buffer() {
  if(!buffer.mdl)return STATUS_SUCCESS;
  if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
  KIRQL irql;KeAcquireSpinLock(&bufferLock,&irql);readable=FALSE;KeReleaseSpinLock(&bufferLock,irql);
  if(state==KSSTATE_RUN){NTSTATUS status=a04_bt_hfp_suspend(session,direction);if(!NT_SUCCESS(status))return status;state=KSSTATE_STOP;}
  NTSTATUS status=p()->free_drained(p()->context,direction,&buffer);
  if(NT_SUCCESS(status))RtlZeroMemory(&buffer,sizeof(buffer));return status;
 }
 static void retired(PVOID context){static_cast<HfpStream *>(context)->Release();}
public:
 HfpStream(struct a04_bt_hfp_session *s,ULONG d,PPORTWAVERTSTREAM pp,const struct a04_bt_hfp_latency *l):refs(1),retiring(0),deferred(0),readable(FALSE),session(s),direction(d),port(pp),buffer{},latency(*l),state(KSSTATE_STOP){KeInitializeSpinLock(&bufferLock);a04_bt_hfp_reference(s);port->AddRef();}
 ~HfpStream(){a04_bt_hfp_pin_release(session,direction);port->Release();a04_bt_hfp_dereference(session);}
 STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id,PVOID *out) override {
  if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
  if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IMiniportWaveRTStream)){*out=static_cast<IMiniportWaveRTStream *>(this);AddRef();return STATUS_SUCCESS;}return STATUS_NOINTERFACE;
 }
 STDMETHODIMP_(ULONG) AddRef() override {return InterlockedIncrement(&refs);}
 STDMETHODIMP_(ULONG) Release() override {
  LONG n=InterlockedDecrement(&refs);if(n)return (ULONG)n;
  if(KeGetCurrentIrql()!=PASSIVE_LEVEL){
   /* COM final Release at DISPATCH is normal. Defer real ring/port teardown,
    * rather than report a fake DMA fault or wait for the peer pin to stop. */
   InterlockedExchange(&refs,1);
   if(!InterlockedCompareExchange(&deferred,1,0)){
    NTSTATUS queued=a04_bt_hfp_defer_cleanup(session,retired,this);
    if(!NT_SUCCESS(queued))p()->fault(p()->context,queued);
   }
   return 1;
  }
  NTSTATUS status=free_buffer();
  if(!NT_SUCCESS(status)){
   InterlockedExchange(&refs,1);p()->fault(p()->context,status);
   if(!InterlockedCompareExchange(&retiring,1,0))(void)p()->retire(p()->context,retired,this);
   return 1; /* Actual DMA ownership uncertain: retain object, MDL and module. */
  }
  this->~HfpStream();ExFreePoolWithTag(this,TAG);return 0;
 }
 STDMETHODIMP_(NTSTATUS) SetFormat(PKSDATAFORMAT f) override {
  NTSTATUS status=a04_bt_hfp_status(session);if(!NT_SUCCESS(status))return status;
  return state==KSSTATE_STOP&&format_ok(f)?STATUS_SUCCESS:STATUS_NO_MATCH;
 }
 STDMETHODIMP_(NTSTATUS) SetState(KSSTATE next) override {
  if(KeGetCurrentIrql()!=PASSIVE_LEVEL||next<KSSTATE_STOP||next>KSSTATE_RUN)return STATUS_INVALID_PARAMETER;
  if(next==KSSTATE_RUN){
   /* PnP detach/fault can stop the shared session while an old KS handle
    * still retains state==RUN. Equality is not proof that this ring is live. */
   NTSTATUS status=a04_bt_hfp_status(session);if(!NT_SUCCESS(status))return status;
   KIRQL irql;KeAcquireSpinLock(&bufferLock,&irql);
   BOOLEAN live=readable&&buffer.mdl&&p()->buffer_live(p()->context,direction,buffer.generation);
   KeReleaseSpinLock(&bufferLock,irql);if(!live)return STATUS_DEVICE_NOT_READY;
   if(next==state)return STATUS_SUCCESS;
   status=a04_bt_hfp_run(session,direction);if(!NT_SUCCESS(status))return status;
  }else {
   /* Cleanup remains available after detach/fault; do not gate STOP/PAUSE
    * on the new-RUN admission check or turn a failed drain into success. */
   if(next==state)return STATUS_SUCCESS;
   if(state==KSSTATE_RUN){NTSTATUS status=a04_bt_hfp_suspend(session,direction);if(!NT_SUCCESS(status))return status;}
  }
  state=next;return STATUS_SUCCESS;
 }
 STDMETHODIMP_(NTSTATUS) GetPosition(PKSAUDIO_POSITION out) override {
  if(!out)return STATUS_INVALID_PARAMETER;
  KIRQL irql;KeAcquireSpinLock(&bufferLock,&irql);
  NTSTATUS status=a04_bt_hfp_status(session);ULONGLONG play=0,write=0;
  if(NT_SUCCESS(status)&&(!readable||!buffer.mdl||!p()->buffer_live(p()->context,direction,buffer.generation)))status=STATUS_DEVICE_NOT_READY;
  if(NT_SUCCESS(status))status=p()->position(p()->context,direction,buffer.generation,&play,&write);
  if(NT_SUCCESS(status)&&(play>=buffer.bytes||write>=buffer.bytes||(play&1)||(write&1)))status=STATUS_DATA_ERROR;
  if(NT_SUCCESS(status)){out->PlayOffset=play;out->WriteOffset=write;}
  KeReleaseSpinLock(&bufferLock,irql);return status;
 }
 STDMETHODIMP_(NTSTATUS) AllocateAudioBuffer(ULONG requested,PMDL *mdl,PULONG actual,PULONG offset,MEMORY_CACHING_TYPE *cache) override {
  if(!mdl||!actual||!offset||!cache||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
  *mdl=NULL;*actual=*offset=0;if(buffer.mdl||state==KSSTATE_RUN||!requested||(requested&1)||requested>65536)return STATUS_INVALID_PARAMETER;
  NTSTATUS status=a04_bt_hfp_status(session);if(!NT_SUCCESS(status))return status;
  status=p()->allocate(p()->context,direction,requested,&buffer);
  if(!NT_SUCCESS(status)){if(buffer.mdl)p()->fault(p()->context,status);return status;}
  if(!buffer.mdl||!buffer.generation||buffer.bytes<requested||buffer.bytes>65536||(buffer.bytes&1)||
     buffer.cache!=MmNonCached||(buffer.bytes&(PAGE_SIZE-1))||MmGetMdlByteOffset(buffer.mdl)||MmGetMdlByteCount(buffer.mdl)!=buffer.bytes||!p()->buffer_live(p()->context,direction,buffer.generation)){
   p()->fault(p()->context,STATUS_DEVICE_CONFIGURATION_ERROR);return STATUS_DEVICE_CONFIGURATION_ERROR;
  }
  KIRQL irql;KeAcquireSpinLock(&bufferLock,&irql);readable=TRUE;KeReleaseSpinLock(&bufferLock,irql);
  *mdl=buffer.mdl;*actual=buffer.bytes;*cache=buffer.cache;return STATUS_SUCCESS;
 }
 STDMETHODIMP_(VOID) FreeAudioBuffer(PMDL mdl,ULONG bytes) override {
  if(!buffer.mdl)return;
  if(mdl!=buffer.mdl||bytes!=buffer.bytes){p()->fault(p()->context,STATUS_INVALID_PARAMETER);return;}
  NTSTATUS status=free_buffer();if(!NT_SUCCESS(status))p()->fault(p()->context,status);
 }
 STDMETHODIMP_(VOID) GetHWLatency(KSRTAUDIO_HWLATENCY *out) override {if(out){out->FifoSize=latency.fifo;out->ChipsetDelay=latency.chipset;out->CodecDelay=latency.codec;}}
 STDMETHODIMP_(NTSTATUS) GetPositionRegister(KSRTAUDIO_HWREGISTER *) override {return STATUS_NOT_SUPPORTED;}
 STDMETHODIMP_(NTSTATUS) GetClockRegister(KSRTAUDIO_HWREGISTER *) override {return STATUS_NOT_SUPPORTED;}
};
class HfpMiniport : public IMiniportWaveRT {
 volatile LONG refs;struct a04_bt_hfp_session *session;ULONG direction;
public:
 HfpMiniport(struct a04_bt_hfp_session *s,ULONG d):refs(1),session(s),direction(d){a04_bt_hfp_reference(s);}
 ~HfpMiniport(){a04_bt_hfp_dereference(session);}
 STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id,PVOID *out) override {
  if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
  if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IMiniport)||IsEqualGUIDAligned(id,IID_IMiniportWaveRT)){*out=static_cast<IMiniportWaveRT *>(this);AddRef();return STATUS_SUCCESS;}return STATUS_NOINTERFACE;
 }
 STDMETHODIMP_(ULONG) AddRef() override {return InterlockedIncrement(&refs);}
 STDMETHODIMP_(ULONG) Release() override {LONG n=InterlockedDecrement(&refs);if(!n){this->~HfpMiniport();ExFreePoolWithTag(this,TAG);}return (ULONG)n;}
 STDMETHODIMP_(NTSTATUS) GetDescription(PPCFILTER_DESCRIPTOR *out) override {if(!out)return STATUS_INVALID_PARAMETER;*out=&filters[direction];return STATUS_SUCCESS;}
 STDMETHODIMP_(NTSTATUS) Init(PUNKNOWN,PRESOURCELIST,PPORTWAVERT port) override {return port?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;}
 STDMETHODIMP_(NTSTATUS) GetDeviceDescription(PDEVICE_DESCRIPTION out) override {
  if(!out)return STATUS_INVALID_PARAMETER;NTSTATUS status=a04_bt_hfp_status(session);if(!NT_SUCCESS(status))return status;
  const struct a04_bt_hfp_provider *p=a04_bt_hfp_provider(session);return p->device_description(p->context,out);
 }
 STDMETHODIMP_(NTSTATUS) NewStream(PMINIPORTWAVERTSTREAM *out,PPORTWAVERTSTREAM port,ULONG pin,BOOLEAN capture,PKSDATAFORMAT f) override {
  if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;if(!port||pin||!!capture!=!!direction||!format_ok(f))return STATUS_NO_MATCH;
  NTSTATUS status=a04_bt_hfp_pin_claim(session,direction);if(!NT_SUCCESS(status))return status;
  const struct a04_bt_hfp_provider *p=a04_bt_hfp_provider(session);struct a04_bt_hfp_latency latency;
  status=p->latency(p->context,direction,&latency);if(!NT_SUCCESS(status)){a04_bt_hfp_pin_release(session,direction);return status;}
  void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(HfpStream),TAG);
  if(!mem){a04_bt_hfp_pin_release(session,direction);return STATUS_INSUFFICIENT_RESOURCES;}
  *out=new(mem,BtPlacement{}) HfpStream(session,direction,port,&latency);return STATUS_SUCCESS;
 }
 STDMETHODIMP_(NTSTATUS) DataRangeIntersection(ULONG pin,PKSDATARANGE requested,PKSDATARANGE matching,ULONG bytes,PVOID output,PULONG required) override {
  if(!required||!requested||!matching||pin)return STATUS_INVALID_PARAMETER;*required=0;
  if(requested->FormatSize<sizeof(KSDATARANGE_AUDIO)||matching->FormatSize<sizeof(KSDATARANGE_AUDIO)||
     !IsEqualGUIDAligned(requested->MajorFormat,KSDATAFORMAT_TYPE_AUDIO)||!IsEqualGUIDAligned(requested->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)||
     !IsEqualGUIDAligned(requested->Specifier,KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))return STATUS_NO_MATCH;
  KSDATARANGE_AUDIO *r=(KSDATARANGE_AUDIO *)requested;
  if(r->MaximumChannels<1||r->MinimumBitsPerSample>16||r->MaximumBitsPerSample<16||r->MinimumSampleFrequency>8000||r->MaximumSampleFrequency<8000)return STATUS_NO_MATCH;
  *required=sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);if(!bytes)return STATUS_BUFFER_OVERFLOW;if(!output||bytes<*required)return STATUS_BUFFER_TOO_SMALL;
  KSDATAFORMAT_WAVEFORMATEXTENSIBLE *f=(KSDATAFORMAT_WAVEFORMATEXTENSIBLE *)output;RtlZeroMemory(f,sizeof(*f));
  f->DataFormat.FormatSize=sizeof(*f);f->DataFormat.SampleSize=2;f->DataFormat.MajorFormat=KSDATAFORMAT_TYPE_AUDIO;
  f->DataFormat.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;f->DataFormat.Specifier=KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
  f->WaveFormatExt.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;f->WaveFormatExt.Format.nChannels=1;f->WaveFormatExt.Format.nSamplesPerSec=8000;
  f->WaveFormatExt.Format.nAvgBytesPerSec=16000;f->WaveFormatExt.Format.nBlockAlign=2;f->WaveFormatExt.Format.wBitsPerSample=16;
  f->WaveFormatExt.Format.cbSize=22;f->WaveFormatExt.Samples.wValidBitsPerSample=16;f->WaveFormatExt.dwChannelMask=KSAUDIO_SPEAKER_MONO;
  f->WaveFormatExt.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;return STATUS_SUCCESS;
 }
};
NTSTATUS Mt6630NewHfpMiniport(struct a04_bt_hfp_session *session,ULONG direction,PMINIPORTWAVERT *out)
{
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;if(!session||direction>1||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 NTSTATUS status=a04_bt_hfp_status(session);if(!NT_SUCCESS(status))return status;
 void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(HfpMiniport),TAG);if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
 *out=new(mem,BtPlacement{}) HfpMiniport(session,direction);return STATUS_SUCCESS;
}
