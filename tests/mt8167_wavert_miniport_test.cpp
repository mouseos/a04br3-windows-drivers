#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#include "wavert_miniport.h"
#include "topology_miniport.h"
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
const GUID KSNODETYPE_ADC={25},KSNODETYPE_MICROPHONE={26};
#endif
const GUID KSNODETYPE_SPEAKER={23},IID_IMiniportTopology={24};
const GUID KSDATAFORMAT_SUBTYPE_ANALOG={20},KSDATAFORMAT_SPECIFIER_NONE={21},KSNODETYPE_DAC={22};
#endif
#include <cassert>
#include <cstring>
#include "wavert_miniport.h"
const GUID IID_IUnknown{1},IID_IMiniport{2},IID_IMiniportWaveRT{3},IID_IMiniportWaveRTStream{4};
const GUID KSDATAFORMAT_TYPE_AUDIO{5},KSDATAFORMAT_SUBTYPE_PCM{6},KSDATAFORMAT_SPECIFIER_WAVEFORMATEX{7},KSCATEGORY_AUDIO{8};
#include "wavert_cache.h"
NTSTATUS Mt8167VerifyWaveCache(PVOID,ULONG bytes,ULONG first) {
 for(ULONG i=0;i<bytes;i+=PAGE_SIZE)
  assert(mt8167_arm_par_normal_nc((uint64_t)(first+i),first+i));
 return STATUS_SUCCESS;
}
struct Port : IPortWaveRTStream {
 MDL mdl{0,8192}; unsigned char data[8192];int refs=1,freed=0;
 PMDL AllocateContiguousPagesForMdl(PHYSICAL_ADDRESS,PHYSICAL_ADDRESS,size_t) override{return &mdl;}
 ULONG GetPhysicalPagesCount(PMDL) override{return 2;}
 PHYSICAL_ADDRESS GetPhysicalPageAddress(PMDL,ULONG n) override{return {0x100000+(int64_t)n*4096};}
 PVOID MapAllocatedPages(PMDL,int cache) override{assert(cache==MmWriteCombined);return data;}
 void UnmapAllocatedPages(PVOID,PMDL) override{}
 void FreePagesFromMdl(PMDL) override{freed++;}
 ULONG AddRef() override{return ++refs;} ULONG Release() override{return --refs;}
};
struct Back { int refs, frees, starts, stops; bool fail, uncertain; MDL mdl;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
 bool poison=false,checked=false;KSSTATE state=KSSTATE_STOP;PMDL retained=nullptr;
#endif
};
static void ref(void *c){((Back*)c)->refs++;}
static void deref(void *c){((Back*)c)->refs--;}
static NTSTATUS allocate(void *c,ULONG,PMDL *m,PULONG n,PULONG o,MEMORY_CACHING_TYPE *cache){Back*b=(Back*)c;*m=&b->mdl;*n=8192;*o=0;*cache=MmNonCached;return STATUS_SUCCESS;}
static void releasebuf(void *c,PMDL,ULONG){((Back*)c)->frees++;}
static NTSTATUS start(void *c,BOOLEAN *exposed){Back*b=(Back*)c;b->starts++;if(b->fail && !b->uncertain)*exposed=FALSE;return b->fail?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;}
static NTSTATUS stop(void *c){Back*b=(Back*)c;b->stops++;return b->fail?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;}
static NTSTATUS device(void *,PDEVICE_DESCRIPTION d){return d?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;}
static NTSTATUS pos(void *,PKSAUDIO_POSITION p){p->PlayOffset=4;p->WriteOffset=4;return STATUS_SUCCESS;}
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
static NTSTATUS bindwave(void *,PMDL mdl,PVOID mapping,ULONG size){
#if defined(A04_TEST_WAVE_BIND_TIMEOUT) && A04_TEST_WAVE_BIND_TIMEOUT
 (void)mdl;(void)mapping;(void)size;return STATUS_IO_TIMEOUT;
#else
 return mdl && mapping && size==8192?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;
#endif
}
static NTSTATUS unbindwave(void *c,PMDL){((Back*)c)->frees++;return STATUS_SUCCESS;}
#endif
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
static NTSTATUS checkout(void *c,PPORTWAVERTSTREAM port,ULONG bytes,PMDL *mdl,PULONG actual) {
 Back *b=(Back *)c;if(b->checked || b->poison || bytes>8192)return STATUS_DEVICE_NOT_READY;
 if(!b->retained)b->retained=&static_cast<Port *>(port)->mdl;
 b->checked=true;*mdl=b->retained;*actual=8192;return STATUS_SUCCESS;
}
static NTSTATUS checkin(void *c,PPORTWAVERTSTREAM,PMDL) {
 ((Back *)c)->checked=false;return STATUS_SUCCESS;
}
static NTSTATUS state(void *c,PPORTWAVERTSTREAM,KSSTATE s) {
 Back *b=(Back *)c;if(b->poison)return STATUS_DEVICE_NOT_READY;
 if(b->state==s)return STATUS_SUCCESS;
 if(s==KSSTATE_RUN)b->starts++;else if(b->state==KSSTATE_RUN)b->stops++;
 if(b->fail){b->poison=true;return STATUS_DEVICE_NOT_READY;}
 b->state=s;return STATUS_SUCCESS;
}
#endif
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
static NTSTATUS unknownHp(void*,KSRTAUDIO_HWLATENCY *p){p->FifoSize=p->ChipsetDelay=p->CodecDelay=MAXULONG;return STATUS_SUCCESS;}
#endif
int main(){
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
 PMINIPORTTOPOLOGY topo=nullptr;PPCFILTER_DESCRIPTOR topology=nullptr;
 assert(!Mt8167NewTopology(&topo));assert(!topo->GetDescription(&topology));
 assert(topology->pinCount==2 && topology->connectionCount==1);
 assert(topology->pins[1].pin.category==&KSNODETYPE_SPEAKER);topo->Release();
#endif
 PMINIPORTWAVERT m=nullptr; assert(!Mt8167NewMiniport(&m) && m);
 PPCFILTER_DESCRIPTOR d=nullptr;assert(!m->GetDescription(&d) && d->pinCount==2);
 KSDATARANGE_AUDIO range{};range.DataRange.FormatSize=sizeof(range);range.DataRange.MajorFormat=KSDATAFORMAT_TYPE_AUDIO;range.DataRange.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;range.DataRange.Specifier=KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;range.MaximumChannels=2;range.MinimumBitsPerSample=16;range.MaximumBitsPerSample=16;range.MinimumSampleFrequency=48000;range.MaximumSampleFrequency=48000;
 ULONG needed=0; assert(m->DataRangeIntersection(0,&range.DataRange,&range.DataRange,0,nullptr,&needed)==STATUS_BUFFER_OVERFLOW && needed==sizeof(KSDATAFORMAT_WAVEFORMATEX));
 KSDATARANGE_AUDIO unsupported=range; unsupported.MinimumSampleFrequency=96000;
 assert(m->DataRangeIntersection(0,&range.DataRange,&unsupported.DataRange,0,nullptr,&needed)==STATUS_NO_MATCH);
 KSDATAFORMAT_WAVEFORMATEX fmt{};assert(!m->DataRangeIntersection(0,&range.DataRange,&range.DataRange,sizeof(fmt),&fmt,&needed));
 Port p; PMINIPORTWAVERTSTREAM st=nullptr;assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat));
 assert(st->SetState(KSSTATE_RUN)==STATUS_DEVICE_NOT_READY); assert(!st->SetState(KSSTATE_PAUSE));
 PMDL mdl=nullptr;ULONG actual=0,off=0;MEMORY_CACHING_TYPE cache;assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache) && actual==8192 && mdl);
 st->FreeAudioBuffer(mdl,actual); assert(p.freed==1);
 assert(!st->AllocateAudioBuffer(4097,&mdl,&actual,&off,&cache) && actual==8192 && !off);
 st->FreeAudioBuffer(mdl,actual);assert(p.freed==2);st->Release();assert(p.refs==1);
 fmt.WaveFormatEx.nChannels=1; assert(m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat)==STATUS_NO_MATCH && !st);m->Release();
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
 assert(d->nodeCount==1 && d->connectionCount==2);
#endif
 fmt.WaveFormatEx.nChannels=2; Back b{};Mt8167StreamBackend backend{&b,ref,deref,allocate,releasebuf,start,stop,pos,device,nullptr,nullptr
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
 ,checkout,checkin,state
#endif
 };
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
 backend.bindWaveBuffer=bindwave;backend.unbindWaveBuffer=unbindwave;
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
 backend.capture=TRUE;backend.captureLatency.FifoSize=MAXULONG;
 assert(Mt8167NewSpeakerMiniportWithBackend(&m,&backend)==STATUS_INVALID_PARAMETER&&!m);
 assert(!Mt8167NewMiniportWithBackend(&m,&backend));
 assert(m->NewStream(&st,&p,0,TRUE,&fmt.DataFormat)==STATUS_NOT_SUPPORTED&&!st);
 m->Release();backend.capture=FALSE;backend.captureLatency.FifoSize=0;
#endif
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
 backend.prepareRenderStream=unknownHp;
 assert(Mt8167NewSpeakerMiniportWithBackend(&m,&backend)==STATUS_INVALID_PARAMETER&&!m);
 assert(!Mt8167NewMiniportWithBackend(&m,&backend));
 assert(m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat)==STATUS_NOT_SUPPORTED&&!st);
 m->Release();backend.prepareRenderStream=nullptr;
#endif
 /* Real speaker factory selects vendor nominal model per instance; generic
  * factory and HP/capture must not inherit a device-wide latency exemption. */
 assert(Mt8167NewSpeakerMiniportWithBackend(&m,nullptr)==STATUS_INVALID_PARAMETER);
 assert(!Mt8167NewSpeakerMiniportWithBackend(&m,&backend));
 assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat));
 KSRTAUDIO_HWLATENCY nominal;memset(&nominal,0xff,sizeof(nominal));
 st->GetHWLatency(&nominal);assert(nominal.FifoSize==0&&nominal.ChipsetDelay==0&&nominal.CodecDelay==0);
 st->Release();m->Release();assert(!b.refs);
 assert(!Mt8167NewMiniportWithBackend(&m,&backend) && b.refs==1);assert(!m->GetDeviceDescription(&b));
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
 struct ExtFormat { KSDATAFORMAT data;WAVEFORMATEXTENSIBLE wave; } ext{};
 ext.data=fmt.DataFormat;ext.data.FormatSize=sizeof(ext);
 ext.wave.Format=fmt.WaveFormatEx;ext.wave.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
 ext.wave.Format.cbSize=22;ext.wave.Samples.wValidBitsPerSample=16;
 ext.wave.dwChannelMask=3;ext.wave.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
 assert(!m->NewStream(&st,&p,0,FALSE,&ext.data));st->Release();
 ext.wave.dwChannelMask=4;
 assert(m->NewStream(&st,&p,0,FALSE,&ext.data)==STATUS_NO_MATCH);
#endif
 assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat) && b.refs==2);
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
 assert(!st->SetState(KSSTATE_ACQUIRE));
 assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache) && cache==MmWriteCombined);
 assert(!st->SetState(KSSTATE_PAUSE) && !st->SetState(KSSTATE_RUN));
 assert(!st->SetState(KSSTATE_PAUSE) && !st->SetState(KSSTATE_RUN));
 assert(!st->SetState(KSSTATE_STOP) && !st->SetState(KSSTATE_RUN));
 st->FreeAudioBuffer(mdl,actual);assert(!b.checked && p.freed==2);
 st->Release();Port next;
 assert(!m->NewStream(&st,&next,0,FALSE,&fmt.DataFormat));
 PMDL again=nullptr;assert(!st->AllocateAudioBuffer(8192,&again,&actual,&off,&cache) && again==mdl);
 assert(!st->SetState(KSSTATE_RUN));b.fail=true;
 assert(st->SetState(KSSTATE_PAUSE)==STATUS_DEVICE_NOT_READY);
 b.fail=false;assert(st->SetState(KSSTATE_RUN)==STATUS_DEVICE_NOT_READY);
 st->Release();m->Release();assert(!b.refs && !b.frees);
 return 0;
#else
#if defined(A04_TEST_WAVE_BIND_TIMEOUT) && A04_TEST_WAVE_BIND_TIMEOUT
 {int freed=p.freed;assert(st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache)==STATUS_IO_TIMEOUT && !mdl);
  st->Release();m->Release();assert(b.refs==1 && !b.frees && p.freed==freed);return 0;}
#endif
 assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache) && cache==MmWriteCombined);
#if (defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC) || (defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC)
 assert(mdl==&p.mdl && mdl!=&b.mdl);
#endif
 assert(!st->SetState(KSSTATE_RUN));KSAUDIO_POSITION position{};assert(!st->GetPosition(&position) && position.PlayOffset==4);
 assert(!st->SetState(KSSTATE_STOP));st->FreeAudioBuffer(mdl,actual);
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED && (!defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM)
 assert(b.frees==1);
#else
 assert(!b.frees);
#endif
 assert(st->SetState(KSSTATE_RUN)==STATUS_DEVICE_NOT_READY);st->Release();m->Release();
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED && (!defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM)
 assert(!b.refs && b.starts==1 && b.stops==1 && b.frees==1);
#else
 assert(b.refs==1 && b.starts==1 && b.stops==1 && !b.frees);
#endif
 Back b2{};b2.fail=true;backend.context=&b2;assert(!Mt8167NewMiniportWithBackend(&m,&backend));
 assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat));assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache) && cache==MmWriteCombined);
 assert(st->SetState(KSSTATE_RUN)==STATUS_DEVICE_NOT_READY);st->FreeAudioBuffer(mdl,actual);st->Release();m->Release();
 assert(!b2.refs && b2.frees==1);
 Back b3{};backend.context=&b3;assert(!Mt8167NewMiniportWithBackend(&m,&backend));
 assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat));assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache) && cache==MmWriteCombined);
 assert(!st->SetState(KSSTATE_RUN));b3.fail=true;assert(st->SetState(KSSTATE_STOP)==STATUS_DEVICE_NOT_READY);
 b3.fail=false;assert(st->SetState(KSSTATE_STOP)==STATUS_DEVICE_NOT_READY);assert(st->SetState(KSSTATE_PAUSE)==STATUS_DEVICE_NOT_READY);
 st->Release();m->Release();assert(b3.stops==1 && b3.refs==1 && !b3.frees);
 Back b4{};b4.fail=true;b4.uncertain=true;backend.context=&b4;assert(!Mt8167NewMiniportWithBackend(&m,&backend));
 assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat));assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache) && cache==MmWriteCombined);
 assert(st->SetState(KSSTATE_RUN)==STATUS_DEVICE_NOT_READY);assert(st->SetState(KSSTATE_STOP)==STATUS_DEVICE_NOT_READY);st->FreeAudioBuffer(mdl,actual);st->Release();m->Release();
 assert(b4.refs==1 && !b4.frees);
#endif
}
