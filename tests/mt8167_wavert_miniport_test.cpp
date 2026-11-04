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
}
