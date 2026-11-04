#include "wavert_miniport.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
#include "wavert_dma.h"
#define AUDIO_TAG '7auM'
void *operator new(size_t, void *p) { return p; }
static bool ValidFormat(PKSDATAFORMAT f)
{
    if (!f || f->FormatSize < sizeof(KSDATAFORMAT_WAVEFORMATEX) ||
        !IsEqualGUIDAligned(f->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(f->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUIDAligned(f->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) return false;
    const WAVEFORMATEX *w = &reinterpret_cast<PKSDATAFORMAT_WAVEFORMATEX>(f)->WaveFormatEx;
    return w->wFormatTag == WAVE_FORMAT_PCM && w->nChannels == 2 &&
        w->nSamplesPerSec == 48000 && w->wBitsPerSample == 16 &&
        w->nBlockAlign == 4 && w->nAvgBytesPerSec == 192000 && !w->cbSize;
}
static KSDATARANGE_AUDIO Range = {
    {sizeof(KSDATARANGE_AUDIO),0,0,0, STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
     STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM), STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)},
    2,16,16,48000,48000
};
static PKSDATARANGE Ranges[] = {reinterpret_cast<PKSDATARANGE>(&Range)};
static PCPIN_DESCRIPTOR Pins[] = {
    {1,1,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,NULL,0}},
    {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}
};
static PCCONNECTION_DESCRIPTOR Connections[] = {{PCFILTER_NODE,0,PCFILTER_NODE,1}};
static PCFILTER_DESCRIPTOR Filter = {0,NULL,sizeof(PCPIN_DESCRIPTOR),2,Pins,
    sizeof(PCNODE_DESCRIPTOR),0,NULL,1,Connections,0,NULL};
class Stream : public IMiniportWaveRTStream {
    volatile LONG refs;
    PPORTWAVERTSTREAM port;
    Mt8167WaveBuffer buffer;
public:
    explicit Stream(PPORTWAVERTSTREAM p): refs(1),port(p),buffer{} { port->AddRef(); }
    ~Stream() { if (buffer.mdl && !buffer.handedToDevice) Mt8167ReleaseUnpublishedBuffer(&buffer); port->Release(); }
    STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id, PVOID *out) override {
        if (!out) return STATUS_INVALID_PARAMETER; *out=NULL;
        if (IsEqualGUIDAligned(id,IID_IUnknown) || IsEqualGUIDAligned(id,IID_IMiniportWaveRTStream)) {
            *out=static_cast<IMiniportWaveRTStream *>(this); AddRef(); return STATUS_SUCCESS;
        } return STATUS_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs); }
    STDMETHODIMP_(ULONG) Release() override { LONG n=InterlockedDecrement(&refs); if(!n) { this->~Stream(); ExFreePoolWithTag(this,AUDIO_TAG); } return (ULONG)n; }
    STDMETHODIMP_(NTSTATUS) SetFormat(PKSDATAFORMAT f) override { return ValidFormat(f)?STATUS_SUCCESS:STATUS_NO_MATCH; }
    STDMETHODIMP_(NTSTATUS) SetState(KSSTATE s) override {
        if (s==KSSTATE_RUN) return STATUS_DEVICE_NOT_READY;
        return s==KSSTATE_STOP || s==KSSTATE_ACQUIRE || s==KSSTATE_PAUSE ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
    }
    STDMETHODIMP_(NTSTATUS) GetPosition(PKSAUDIO_POSITION p) override { if(!p)return STATUS_INVALID_PARAMETER; p->PlayOffset=0;p->WriteOffset=0;return STATUS_SUCCESS; }
    STDMETHODIMP_(NTSTATUS) AllocateAudioBuffer(ULONG size,PMDL *mdl,PULONG actual,PULONG offset,MEMORY_CACHING_TYPE *cache) override {
        if(!mdl || !actual || !offset || !cache) return STATUS_INVALID_PARAMETER;
        *mdl=NULL;*actual=0;*offset=0;
        NTSTATUS status=Mt8167AllocateBuffer(port,size,&buffer); if(!NT_SUCCESS(status)) return status;
        *mdl=buffer.mdl;*actual=buffer.bytes;*cache=MmNonCached;return STATUS_SUCCESS;
    }
    STDMETHODIMP_(VOID) FreeAudioBuffer(PMDL mdl,ULONG size) override {
        if (mdl && mdl==buffer.mdl && size==buffer.bytes) (void)Mt8167ReleaseUnpublishedBuffer(&buffer);
    }
    STDMETHODIMP_(VOID) GetHWLatency(KSRTAUDIO_HWLATENCY *p) override { if(p) RtlZeroMemory(p,sizeof(*p)); }
    STDMETHODIMP_(NTSTATUS) GetPositionRegister(KSRTAUDIO_HWREGISTER *) override { return STATUS_NOT_SUPPORTED; }
    STDMETHODIMP_(NTSTATUS) GetClockRegister(KSRTAUDIO_HWREGISTER *) override { return STATUS_NOT_SUPPORTED; }
};
class Miniport : public IMiniportWaveRT {
    volatile LONG refs;
public:
    Miniport():refs(1) {}
    STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id, PVOID *out) override {
        if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
        if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IMiniport)||IsEqualGUIDAligned(id,IID_IMiniportWaveRT)) {
            *out=static_cast<IMiniportWaveRT *>(this);AddRef();return STATUS_SUCCESS;
        } return STATUS_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs); }
    STDMETHODIMP_(ULONG) Release() override { LONG n=InterlockedDecrement(&refs);if(!n){this->~Miniport();ExFreePoolWithTag(this,AUDIO_TAG);}return (ULONG)n; }
    STDMETHODIMP_(NTSTATUS) GetDescription(PPCFILTER_DESCRIPTOR *p) override { if(!p)return STATUS_INVALID_PARAMETER;*p=&Filter;return STATUS_SUCCESS; }
    STDMETHODIMP_(NTSTATUS) DataRangeIntersection(ULONG pin, PKSDATARANGE requested,
        PKSDATARANGE matching, ULONG bytes, PVOID output, PULONG required) override {
        if (!required || !requested || !matching || pin != 0) return STATUS_INVALID_PARAMETER;
        *required = 0;
        if (requested->FormatSize < sizeof(KSDATARANGE_AUDIO) ||
            matching->FormatSize < sizeof(KSDATARANGE_AUDIO) ||
            !IsEqualGUIDAligned(requested->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
            !IsEqualGUIDAligned(requested->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
            !IsEqualGUIDAligned(requested->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) ||
            !IsEqualGUIDAligned(matching->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
            !IsEqualGUIDAligned(matching->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
            !IsEqualGUIDAligned(matching->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) return STATUS_NO_MATCH;
        const KSDATARANGE_AUDIO *range = reinterpret_cast<KSDATARANGE_AUDIO *>(requested);
        const KSDATARANGE_AUDIO *supported = reinterpret_cast<KSDATARANGE_AUDIO *>(matching);
        if (range->MaximumChannels < 2 || range->MinimumBitsPerSample > 16 ||
            range->MaximumBitsPerSample < 16 || range->MinimumSampleFrequency > 48000 ||
            range->MaximumSampleFrequency < 48000 || supported->MaximumChannels < 2 ||
            supported->MinimumBitsPerSample > 16 || supported->MaximumBitsPerSample < 16 ||
            supported->MinimumSampleFrequency > 48000 || supported->MaximumSampleFrequency < 48000)
            return STATUS_NO_MATCH;
        *required = sizeof(KSDATAFORMAT_WAVEFORMATEX);
        if (!bytes) return STATUS_BUFFER_OVERFLOW;
        if (!output || bytes < *required) return STATUS_BUFFER_TOO_SMALL;
        PKSDATAFORMAT_WAVEFORMATEX f = static_cast<PKSDATAFORMAT_WAVEFORMATEX>(output);
        RtlZeroMemory(f, sizeof(*f));
        f->DataFormat.FormatSize = sizeof(*f);
        f->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
        f->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        f->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
        f->DataFormat.SampleSize = 4;
        f->WaveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
        f->WaveFormatEx.nChannels = 2;
        f->WaveFormatEx.nSamplesPerSec = 48000;
        f->WaveFormatEx.nAvgBytesPerSec = 192000;
        f->WaveFormatEx.nBlockAlign = 4;
        f->WaveFormatEx.wBitsPerSample = 16;
        return STATUS_SUCCESS;
    }
    STDMETHODIMP_(NTSTATUS) Init(PUNKNOWN,PRESOURCELIST,PPORTWAVERT p) override { return p?STATUS_SUCCESS:STATUS_INVALID_PARAMETER; }
    STDMETHODIMP_(NTSTATUS) NewStream(PMINIPORTWAVERTSTREAM *out,PPORTWAVERTSTREAM port,ULONG pin,BOOLEAN capture,PKSDATAFORMAT format) override {
        if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
        if(!port || pin!=0 || capture || !ValidFormat(format))return STATUS_NO_MATCH;
        void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(Stream),AUDIO_TAG);if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
        *out=new(mem) Stream(port);return STATUS_SUCCESS;
    }
    STDMETHODIMP_(NTSTATUS) GetDeviceDescription(PDEVICE_DESCRIPTION) override { return STATUS_DEVICE_NOT_READY; }
};
NTSTATUS Mt8167NewMiniport(PMINIPORTWAVERT *out) {
    if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
    void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(Miniport),AUDIO_TAG);if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
    *out=new(mem) Miniport();return STATUS_SUCCESS;
}
#endif
