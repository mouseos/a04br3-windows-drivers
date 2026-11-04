#include <cassert>
#include <cstring>
#include "wavert_miniport.h"
const GUID IID_IUnknown{1},IID_IMiniport{2},IID_IMiniportWaveRT{3},IID_IMiniportWaveRTStream{4};
const GUID KSDATAFORMAT_TYPE_AUDIO{5},KSDATAFORMAT_SUBTYPE_PCM{6},KSDATAFORMAT_SPECIFIER_WAVEFORMATEX{7},KSCATEGORY_AUDIO{8};
struct Port : IPortWaveRTStream {
 MDL mdl{0,8192}; unsigned char data[8192];int refs=1,freed=0;
 PMDL AllocateContiguousPagesForMdl(PHYSICAL_ADDRESS,PHYSICAL_ADDRESS,size_t) override{return &mdl;}
 ULONG GetPhysicalPagesCount(PMDL) override{return 2;}
 PHYSICAL_ADDRESS GetPhysicalPageAddress(PMDL,ULONG n) override{return {0x100000+(int64_t)n*4096};}
 PVOID MapAllocatedPages(PMDL,int) override{return data;}
 void UnmapAllocatedPages(PVOID,PMDL) override{}
 void FreePagesFromMdl(PMDL) override{freed++;}
 ULONG AddRef() override{return ++refs;} ULONG Release() override{return --refs;}
};
struct Back { int refs, frees, starts, stops; bool fail; MDL mdl; };
static void ref(void *c){((Back*)c)->refs++;}
static void deref(void *c){((Back*)c)->refs--;}
static NTSTATUS allocate(void *c,ULONG,PMDL *m,PULONG n,PULONG o,MEMORY_CACHING_TYPE *cache){Back*b=(Back*)c;*m=&b->mdl;*n=8192;*o=0;*cache=MmNonCached;return STATUS_SUCCESS;}
static void releasebuf(void *c,PMDL,ULONG){((Back*)c)->frees++;}
static NTSTATUS start(void *c){Back*b=(Back*)c;b->starts++;return b->fail?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;}
static NTSTATUS stop(void *c){Back*b=(Back*)c;b->stops++;return b->fail?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;}
static NTSTATUS device(void *,PDEVICE_DESCRIPTION d){return d?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;}
static NTSTATUS pos(void *,PKSAUDIO_POSITION p){p->PlayOffset=4;p->WriteOffset=4;return STATUS_SUCCESS;}
int main(){
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
 st->FreeAudioBuffer(mdl,actual); assert(p.freed==1);st->Release();assert(p.refs==1);
 fmt.WaveFormatEx.nChannels=1; assert(m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat)==STATUS_NO_MATCH && !st);m->Release();
 fmt.WaveFormatEx.nChannels=2; Back b{};Mt8167StreamBackend backend{&b,ref,deref,allocate,releasebuf,start,stop,pos,device};
 assert(!Mt8167NewMiniportWithBackend(&m,&backend) && b.refs==1);assert(!m->GetDeviceDescription(&b));
 assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat) && b.refs==2);
 assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache));
 assert(!st->SetState(KSSTATE_RUN));KSAUDIO_POSITION position{};assert(!st->GetPosition(&position) && position.PlayOffset==4);
 assert(!st->SetState(KSSTATE_STOP));st->FreeAudioBuffer(mdl,actual);assert(!b.frees);
 assert(st->SetState(KSSTATE_RUN)==STATUS_DEVICE_NOT_READY);st->Release();m->Release();
 assert(b.refs==1 && b.starts==1 && b.stops==1 && !b.frees);
 Back b2{};b2.fail=true;backend.context=&b2;assert(!Mt8167NewMiniportWithBackend(&m,&backend));
 assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat));assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache));
 assert(st->SetState(KSSTATE_RUN)==STATUS_DEVICE_NOT_READY);st->FreeAudioBuffer(mdl,actual);st->Release();m->Release();
 assert(b2.refs==1 && !b2.frees);
 Back b3{};backend.context=&b3;assert(!Mt8167NewMiniportWithBackend(&m,&backend));
 assert(!m->NewStream(&st,&p,0,FALSE,&fmt.DataFormat));assert(!st->AllocateAudioBuffer(8192,&mdl,&actual,&off,&cache));
 assert(!st->SetState(KSSTATE_RUN));b3.fail=true;assert(st->SetState(KSSTATE_STOP)==STATUS_DEVICE_NOT_READY);
 st->Release();m->Release();assert(b3.stops==1 && b3.refs==1 && !b3.frees);
}
