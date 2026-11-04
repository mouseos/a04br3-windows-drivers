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
    Mt8167StreamBackend backend;
    bool useBackend;
    PMDL backendMdl;
    ULONG backendBytes;
    bool started;
    bool everStarted;
public:
    explicit Stream(PPORTWAVERTSTREAM p, const Mt8167StreamBackend *b): refs(1),port(p),buffer{},backend{},useBackend(b!=NULL),backendMdl(NULL),backendBytes(0),started(false),everStarted(false) {
        port->AddRef(); if (b) { backend=*b; backend.reference(backend.context); }
    }
    ~Stream() {
        if (useBackend) {
            if (started) (void)backend.stop(backend.context);
            /* No drain proof exists. Retain the backend reference and buffer
             * after any attempted start, including an ambiguous failure. */
            if (!everStarted) {
                if (backendMdl) backend.freeBuffer(backend.context,backendMdl,backendBytes);
                backend.dereference(backend.context);
            }
        } else if (buffer.mdl && !buffer.handedToDevice) Mt8167ReleaseUnpublishedBuffer(&buffer);
        port->Release();
    }
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
        if (s==KSSTATE_RUN) {
            if (!useBackend || !backendMdl || started || everStarted) return STATUS_DEVICE_NOT_READY;
            everStarted=true;
            NTSTATUS result=backend.start(backend.context); if (NT_SUCCESS(result)) started=true;
            return result;
        }
        if (s!=KSSTATE_STOP && s!=KSSTATE_ACQUIRE && s!=KSSTATE_PAUSE) return STATUS_INVALID_PARAMETER;
        if (started) { NTSTATUS result=backend.stop(backend.context); started=false; if (!NT_SUCCESS(result)) return result; }
        return STATUS_SUCCESS;
    }
    STDMETHODIMP_(NTSTATUS) GetPosition(PKSAUDIO_POSITION p) override { if(!p)return STATUS_INVALID_PARAMETER; if(useBackend && started)return backend.position(backend.context,p); p->PlayOffset=0;p->WriteOffset=0;return STATUS_SUCCESS; }
    STDMETHODIMP_(NTSTATUS) AllocateAudioBuffer(ULONG size,PMDL *mdl,PULONG actual,PULONG offset,MEMORY_CACHING_TYPE *cache) override {
        if(!mdl || !actual || !offset || !cache) return STATUS_INVALID_PARAMETER;
        *mdl=NULL;*actual=0;*offset=0;
        if (useBackend) {
            if (backendMdl) return STATUS_DEVICE_BUSY;
            NTSTATUS result=backend.allocate(backend.context,size,mdl,actual,offset,cache);
            if (NT_SUCCESS(result)) {
                if (!*mdl || *actual < PAGE_SIZE || *actual > 1048576 || *actual > size ||
                    (*actual & (PAGE_SIZE - 1)) || *offset || *cache != MmNonCached) {
                    if (*mdl) backend.freeBuffer(backend.context,*mdl,*actual);
                    *mdl=NULL; *actual=0; *offset=0;
                    return STATUS_DEVICE_CONFIGURATION_ERROR;
                }
                backendMdl=*mdl;backendBytes=*actual;
            }
            return result;
        }
        NTSTATUS status=Mt8167AllocateBuffer(port,size,&buffer); if(!NT_SUCCESS(status)) return status;
        *mdl=buffer.mdl;*actual=buffer.bytes;*cache=MmNonCached;return STATUS_SUCCESS;
    }
    STDMETHODIMP_(VOID) FreeAudioBuffer(PMDL mdl,ULONG size) override {
        if (useBackend) {
            if (mdl && mdl==backendMdl && size==backendBytes && !everStarted) { backend.freeBuffer(backend.context,mdl,size); backendMdl=NULL;backendBytes=0; }
        } else if (mdl && mdl==buffer.mdl && size==buffer.bytes) (void)Mt8167ReleaseUnpublishedBuffer(&buffer);
    }
    STDMETHODIMP_(VOID) GetHWLatency(KSRTAUDIO_HWLATENCY *p) override { if(p) RtlZeroMemory(p,sizeof(*p)); }
    STDMETHODIMP_(NTSTATUS) GetPositionRegister(KSRTAUDIO_HWREGISTER *) override { return STATUS_NOT_SUPPORTED; }
    STDMETHODIMP_(NTSTATUS) GetClockRegister(KSRTAUDIO_HWREGISTER *) override { return STATUS_NOT_SUPPORTED; }
};
class Miniport : public IMiniportWaveRT {
    volatile LONG refs;
    Mt8167StreamBackend backend;
    bool useBackend;
public:
    explicit Miniport(const Mt8167StreamBackend *b):refs(1),backend{},useBackend(b!=NULL) { if(b) { backend=*b;backend.reference(backend.context); } }
    ~Miniport() { if(useBackend) backend.dereference(backend.context); }
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
        *out=new(mem) Stream(port,useBackend?&backend:NULL);return STATUS_SUCCESS;
    }
    STDMETHODIMP_(NTSTATUS) GetDeviceDescription(PDEVICE_DESCRIPTION d) override {
        if (!d) return STATUS_INVALID_PARAMETER;
        return useBackend ? backend.getDeviceDescription(backend.context,d) : STATUS_DEVICE_NOT_READY;
    }
};
NTSTATUS Mt8167NewMiniportWithBackend(PMINIPORTWAVERT *out, const Mt8167StreamBackend *b) {
    if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
    if(b && (!b->reference || !b->dereference || !b->allocate || !b->freeBuffer || !b->start || !b->stop || !b->position || !b->getDeviceDescription)) return STATUS_INVALID_PARAMETER;
    void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(Miniport),AUDIO_TAG);if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
    *out=new(mem) Miniport(b);return STATUS_SUCCESS;
}
NTSTATUS Mt8167NewMiniport(PMINIPORTWAVERT *out) { return Mt8167NewMiniportWithBackend(out,NULL); }
#endif
