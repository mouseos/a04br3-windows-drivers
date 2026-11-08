#include "wavert_miniport.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
#include "wavert_dma.h"
#include "wavert_cache.h"
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
extern "C" int Mt8167SilentDmaMiniportEnabled(void) { return 1; }
#endif
#include "speaker_nominal_latency.h"
#define AUDIO_TAG '7auM'
void *operator new(size_t, void *p) { return p; }
static bool ValidFormat(PKSDATAFORMAT f)
{
    if (!f || f->FormatSize < sizeof(KSDATAFORMAT_WAVEFORMATEX) ||
        !IsEqualGUIDAligned(f->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(f->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUIDAligned(f->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) return false;
    const WAVEFORMATEX *w = &reinterpret_cast<PKSDATAFORMAT_WAVEFORMATEX>(f)->WaveFormatEx;
    bool pcm = w->wFormatTag == WAVE_FORMAT_PCM && !w->cbSize;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    if(w->wFormatTag==WAVE_FORMAT_EXTENSIBLE && w->cbSize==22 &&
       f->FormatSize>=sizeof(KSDATAFORMAT)+sizeof(WAVEFORMATEXTENSIBLE)) {
        const WAVEFORMATEXTENSIBLE *e=reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(w);
        pcm=e->Samples.wValidBitsPerSample==16 && e->dwChannelMask==3 &&
            IsEqualGUIDAligned(e->SubFormat,KSDATAFORMAT_SUBTYPE_PCM);
    }
#endif
    return pcm && w->nChannels == 2 &&
        w->nSamplesPerSec == 48000 && w->wBitsPerSample == 16 &&
        w->nBlockAlign == 4 && w->nAvgBytesPerSec == 192000;
}
static KSDATARANGE_AUDIO Range = {
    {sizeof(KSDATARANGE_AUDIO),0,0,0, STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
     STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM), STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)},
    2,16,16,48000,48000
};
static PKSDATARANGE Ranges[] = {reinterpret_cast<PKSDATARANGE>(&Range)};
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
static KSDATARANGE AnalogRange = {sizeof(KSDATARANGE),0,0,0,
    STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
    STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)};
static PKSDATARANGE AnalogRanges[]={&AnalogRange};
static PCPIN_DESCRIPTOR Pins[] = {
    {1,1,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,NULL,0}},
    {0,0,0,NULL,{0,NULL,0,NULL,1,AnalogRanges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}
};
static PCNODE_DESCRIPTOR Nodes[]={{0,NULL,&KSNODETYPE_DAC,NULL}};
static PCCONNECTION_DESCRIPTOR Connections[] = {
    {PCFILTER_NODE,0,0,1},{0,0,PCFILTER_NODE,1}
};
static PCFILTER_DESCRIPTOR Filter={0,NULL,sizeof(PCPIN_DESCRIPTOR),2,Pins,
    sizeof(PCNODE_DESCRIPTOR),1,Nodes,2,Connections,0,NULL};
#else
static PCPIN_DESCRIPTOR Pins[] = {
    {1,1,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,NULL,0}},
    {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}
};
static PCCONNECTION_DESCRIPTOR Connections[] = {{PCFILTER_NODE,0,PCFILTER_NODE,1}};
static PCFILTER_DESCRIPTOR Filter = {0,NULL,sizeof(PCPIN_DESCRIPTOR),2,Pins,
    sizeof(PCNODE_DESCRIPTOR),0,NULL,1,Connections,0,NULL};
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
/* Real recording direction: analog bridge 1 -> ADC -> host stream 0. */
static PCPIN_DESCRIPTOR CapturePins[] = {
    {1,1,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,NULL,0}},
    {0,0,0,NULL,{0,NULL,0,NULL,1,AnalogRanges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}
};
static PCNODE_DESCRIPTOR CaptureNodes[]={{0,NULL,&KSNODETYPE_ADC,NULL}};
static PCCONNECTION_DESCRIPTOR CaptureConnections[]={{PCFILTER_NODE,1,0,1},{0,0,PCFILTER_NODE,0}};
static PCFILTER_DESCRIPTOR CaptureFilter={0,NULL,sizeof(PCPIN_DESCRIPTOR),2,CapturePins,
    sizeof(PCNODE_DESCRIPTOR),1,CaptureNodes,2,CaptureConnections,0,NULL};
#endif
class Stream : public IMiniportWaveRTStream {
    bool speakerNominal;
    volatile LONG refs;
    PPORTWAVERTSTREAM port;
    Mt8167WaveBuffer buffer;
    Mt8167StreamBackend backend;
    bool useBackend;
    PMDL backendMdl;
    ULONG backendBytes;
    volatile LONG started;
    bool everStarted;
    bool stopUncertain;
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED && (!defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM)
    bool drained;
#endif
public:
    explicit Stream(PPORTWAVERTSTREAM p, const Mt8167StreamBackend *b,bool nominal): speakerNominal(nominal),refs(1),port(p),buffer{},backend{},useBackend(b!=NULL),backendMdl(NULL),backendBytes(0),started(false),everStarted(false),stopUncertain(false)
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED && (!defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM)
    ,drained(false)
#endif
    {
        port->AddRef(); if (b) { backend=*b; backend.reference(backend.context); }
    }
    ~Stream() {
        if (useBackend) {
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
            if(backendMdl) {
                (void)backend.setState(backend.context,port,KSSTATE_STOP);
                (void)backend.checkin(backend.context,port,backendMdl);
            }
            backend.dereference(backend.context);
#else
            if (InterlockedExchange(&started,0)) {
                NTSTATUS stopResult=backend.stop(backend.context);
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED && (!defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM)
                if(NT_SUCCESS(stopResult))drained=true;
#else
                (void)stopResult;
#endif
            }
            /* No drain proof exists. Retain the backend reference and buffer
             * after any start or failure that may have exposed DMA. */
            if (!everStarted
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED && (!defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM)
                || drained
#endif
               ) {
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
                if (backendMdl && NT_SUCCESS(backend.unbindWaveBuffer(backend.context,backendMdl))) {
                    backendMdl=NULL;(void)Mt8167ReleaseUnpublishedBuffer(&buffer);
                }
                if (!backendMdl) backend.dereference(backend.context);
#else
                if (backendMdl) backend.freeBuffer(backend.context,backendMdl,backendBytes);
                backend.dereference(backend.context);
#endif
            }
#endif
        } else if (buffer.mdl && !buffer.handedToDevice) Mt8167ReleaseUnpublishedBuffer(&buffer);
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
        /* This PCM buffer is separate from the private hardware buffer. */
        if (useBackend && buffer.mdl && !buffer.handedToDevice) (void)Mt8167ReleaseUnpublishedBuffer(&buffer);
#endif
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
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
        if(s!=KSSTATE_STOP && s!=KSSTATE_ACQUIRE && s!=KSSTATE_PAUSE && s!=KSSTATE_RUN)
            return STATUS_INVALID_PARAMETER;
        if(!useBackend)return s==KSSTATE_RUN ? STATUS_DEVICE_NOT_READY : STATUS_SUCCESS;
        if(!backendMdl)return s==KSSTATE_RUN ? STATUS_DEVICE_NOT_READY : STATUS_SUCCESS;
        NTSTATUS result=backend.setState(backend.context,port,s);
        if(NT_SUCCESS(result))InterlockedExchange(&started,s==KSSTATE_RUN);
        return result;
#else
        if (s==KSSTATE_RUN) {
            if (!useBackend || !backendMdl || InterlockedCompareExchange(&started,0,0) || everStarted) return STATUS_DEVICE_NOT_READY;
            BOOLEAN possiblyExposed=TRUE;
            NTSTATUS result=backend.start(backend.context,&possiblyExposed);
            everStarted=NT_SUCCESS(result) || possiblyExposed;
            if (!NT_SUCCESS(result) && possiblyExposed) stopUncertain=true;
            if (NT_SUCCESS(result)) InterlockedExchange(&started,1);
            return result;
        }
        if (s!=KSSTATE_STOP && s!=KSSTATE_ACQUIRE && s!=KSSTATE_PAUSE) return STATUS_INVALID_PARAMETER;
        if (stopUncertain) return STATUS_DEVICE_NOT_READY;
        if (InterlockedExchange(&started,0)) { NTSTATUS result=backend.stop(backend.context); if (!NT_SUCCESS(result)) { stopUncertain=true; return result; }
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED && (!defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM)
            drained=true;
#endif
        }
        return STATUS_SUCCESS;
#endif
    }
    STDMETHODIMP_(NTSTATUS) GetPosition(PKSAUDIO_POSITION p) override {
        if(!p)return STATUS_INVALID_PARAMETER;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
        if(useBackend && backendMdl)return backend.position(backend.context,p);
#else
        if(useBackend && InterlockedCompareExchange(&started,0,0))return backend.position(backend.context,p);
#endif
        p->PlayOffset=0;p->WriteOffset=0;return STATUS_SUCCESS;
    }
    STDMETHODIMP_(NTSTATUS) AllocateAudioBuffer(ULONG size,PMDL *mdl,PULONG actual,PULONG offset,MEMORY_CACHING_TYPE *cache) override {
        if(!mdl || !actual || !offset || !cache) return STATUS_INVALID_PARAMETER;
        *mdl=NULL;*actual=0;*offset=0;
        if (useBackend) {
            if (backendMdl) return STATUS_DEVICE_BUSY;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
            NTSTATUS result=backend.checkout(backend.context,port,size,mdl,actual);
            if(NT_SUCCESS(result)) {
                backendMdl=*mdl;backendBytes=*actual;*offset=0;*cache=MT8167_WAVE_CACHE;
            }
            return result;
#elif defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
            NTSTATUS result=Mt8167AllocateBuffer(port,size,&buffer);
            if (!NT_SUCCESS(result)) return result;
            result=backend.bindWaveBuffer(backend.context,buffer.mdl,buffer.mapping,buffer.bytes);
            if (!NT_SUCCESS(result)) {
#if (defined(A04_AUDIO_WAVE_DMA_PROBE) && A04_AUDIO_WAVE_DMA_PROBE) || (defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED)
                if(result==STATUS_IO_TIMEOUT) {
                    /* Callback can outlive this stream. Retain the backend,
                     * port allocation and MDL even though no client got it. */
                    backendMdl=buffer.mdl;backendBytes=buffer.bytes;everStarted=true;
                    return result;
                }
#endif
                (void)Mt8167ReleaseUnpublishedBuffer(&buffer);return result;
            }
            backendMdl=buffer.mdl;backendBytes=buffer.bytes;
            *mdl=buffer.mdl;*actual=buffer.bytes;*offset=0;*cache=MT8167_WAVE_CACHE;
            return STATUS_SUCCESS;
#else
            NTSTATUS result=backend.allocate(backend.context,size,mdl,actual,offset,cache);
            if (NT_SUCCESS(result)) {
                if (!*mdl || *actual < PAGE_SIZE || *actual > 1048576 || *actual < size ||
                    (*actual & (PAGE_SIZE - 1)) || *offset || *cache != MmNonCached) {
                    if (*mdl) backend.freeBuffer(backend.context,*mdl,*actual);
                    *mdl=NULL; *actual=0; *offset=0;
                    return STATUS_DEVICE_CONFIGURATION_ERROR;
                }
                backendMdl=*mdl;backendBytes=*actual;
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
                /* The backend's common buffer is never mapped to the client.
                 * Present a separate PortCls allocation for PCM writes. */
                NTSTATUS stage=Mt8167AllocateBuffer(port,backendBytes,&buffer);
                if (!NT_SUCCESS(stage)) {
                    backend.freeBuffer(backend.context,backendMdl,backendBytes);
                    backendMdl=NULL;backendBytes=0;*mdl=NULL;*actual=0;*offset=0;return stage;
                }
                *mdl=buffer.mdl;*actual=buffer.bytes;*offset=0;*cache=MT8167_WAVE_CACHE;
#endif
            }
            return result;
#endif
        }
        NTSTATUS status=Mt8167AllocateBuffer(port,size,&buffer); if(!NT_SUCCESS(status)) return status;
        *mdl=buffer.mdl;*actual=buffer.bytes;*cache=MT8167_WAVE_CACHE;return STATUS_SUCCESS;
    }
    STDMETHODIMP_(VOID) FreeAudioBuffer(PMDL mdl,ULONG size) override {
        if (useBackend) {
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
            if(mdl && mdl==backendMdl && size==backendBytes) {
                NTSTATUS stop=backend.setState(backend.context,port,KSSTATE_STOP);
                if(NT_SUCCESS(stop) && NT_SUCCESS(backend.checkin(backend.context,port,mdl))) {
                    backendMdl=NULL;backendBytes=0;InterlockedExchange(&started,0);
                }
            }
#elif defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
            if (mdl && mdl==backendMdl && size==backendBytes && (!everStarted
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED && (!defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM)
                || drained
#endif
                ) && NT_SUCCESS(backend.unbindWaveBuffer(backend.context,mdl))) {
                backendMdl=NULL;backendBytes=0;(void)Mt8167ReleaseUnpublishedBuffer(&buffer);
            }
#elif defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
            if (mdl && mdl==buffer.mdl && size==buffer.bytes && !InterlockedCompareExchange(&started,0,0)) {
                (void)Mt8167ReleaseUnpublishedBuffer(&buffer);
                if (!everStarted && backendMdl) {backend.freeBuffer(backend.context,backendMdl,backendBytes);backendMdl=NULL;backendBytes=0;}
            }
#else
            if (mdl && mdl==backendMdl && size==backendBytes && !everStarted) { backend.freeBuffer(backend.context,mdl,size); backendMdl=NULL;backendBytes=0; }
#endif
        } else if (mdl && mdl==buffer.mdl && size==buffer.bytes) (void)Mt8167ReleaseUnpublishedBuffer(&buffer);
    }
    /* No measured codec/fabric latency is available. The inherited zero
     * values are placeholders, not a calibrated latency contract. */
    STDMETHODIMP_(VOID) GetHWLatency(KSRTAUDIO_HWLATENCY *p) override { if(p){
        if(speakerNominal){Mt8167SpeakerNominalHwLatency(p);return;}
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
        if(useBackend&&backend.renderLatencyValid){*p=backend.renderLatency;return;}
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
        if(useBackend&&backend.capture){*p=backend.captureLatency;return;}
#endif
        RtlZeroMemory(p,sizeof(*p));} }
    STDMETHODIMP_(NTSTATUS) GetPositionRegister(KSRTAUDIO_HWREGISTER *) override { return STATUS_NOT_SUPPORTED; }
    STDMETHODIMP_(NTSTATUS) GetClockRegister(KSRTAUDIO_HWREGISTER *) override { return STATUS_NOT_SUPPORTED; }
};
class Miniport : public IMiniportWaveRT {
    bool speakerNominal;
    volatile LONG refs;
    Mt8167StreamBackend backend;
    bool useBackend;
public:
    explicit Miniport(const Mt8167StreamBackend *b,bool nominal):speakerNominal(nominal),refs(1),backend{},useBackend(b!=NULL) { if(b) { backend=*b;backend.reference(backend.context); } }
    ~Miniport() { if(useBackend) backend.dereference(backend.context); }
    STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id, PVOID *out) override {
        if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
        if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IMiniport)||IsEqualGUIDAligned(id,IID_IMiniportWaveRT)) {
            *out=static_cast<IMiniportWaveRT *>(this);AddRef();return STATUS_SUCCESS;
        } return STATUS_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs); }
    STDMETHODIMP_(ULONG) Release() override { LONG n=InterlockedDecrement(&refs);if(!n){this->~Miniport();ExFreePoolWithTag(this,AUDIO_TAG);}return (ULONG)n; }
    STDMETHODIMP_(NTSTATUS) GetDescription(PPCFILTER_DESCRIPTOR *p) override { if(!p)return STATUS_INVALID_PARAMETER;
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
        *p=useBackend&&backend.capture?&CaptureFilter:&Filter;
#else
        *p=&Filter;
#endif
        return STATUS_SUCCESS; }
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
        if(!port || pin!=0 || !ValidFormat(format))return STATUS_NO_MATCH;
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
        if(!!capture!=!!(useBackend&&backend.capture))return STATUS_NO_MATCH;
#else
        if(capture)return STATUS_NO_MATCH;
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
        /* Unknown latency is not a measured zero. A non-speaker stream must
         * supply its own nominal data; do not globally intercept KS queries. */
        if(useBackend&&backend.capture&&
           (backend.captureLatency.FifoSize==MAXULONG||backend.captureLatency.ChipsetDelay==MAXULONG||backend.captureLatency.CodecDelay==MAXULONG))return STATUS_NOT_SUPPORTED;
#endif
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
        Mt8167StreamBackend current=backend;
        if(useBackend&&current.prepareRenderStream){
            KSRTAUDIO_HWLATENCY latency;RtlZeroMemory(&latency,sizeof(latency));
            NTSTATUS status=current.prepareRenderStream(current.context,&latency);
            if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
            if(latency.FifoSize==MAXULONG||latency.ChipsetDelay==MAXULONG||latency.CodecDelay==MAXULONG)return STATUS_NOT_SUPPORTED;
            current.renderLatency=latency;current.renderLatencyValid=TRUE;
        }
#endif
        void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(Stream),AUDIO_TAG);if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
        *out=new(mem) Stream(port,useBackend?&current:NULL,speakerNominal);
#else
        *out=new(mem) Stream(port,useBackend?&backend:NULL,speakerNominal);
#endif
        return STATUS_SUCCESS;
    }
    STDMETHODIMP_(NTSTATUS) GetDeviceDescription(PDEVICE_DESCRIPTION d) override {
        if (!d) return STATUS_INVALID_PARAMETER;
        return useBackend ? backend.getDeviceDescription(backend.context,d) : STATUS_DEVICE_NOT_READY;
    }
};
static NTSTATUS NewMiniport(PMINIPORTWAVERT *out, const Mt8167StreamBackend *b,bool nominal) {
    if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
    if(b && (!b->reference || !b->dereference || !b->allocate || !b->freeBuffer || !b->start || !b->stop || !b->position || !b->getDeviceDescription)) return STATUS_INVALID_PARAMETER;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    if(b && (!b->checkout || !b->checkin || !b->setState))return STATUS_INVALID_PARAMETER;
#endif
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    if(b && !b->capture && (!b->bindWaveBuffer || !b->unbindWaveBuffer)) return STATUS_INVALID_PARAMETER;
#else
    if(b && (!b->bindWaveBuffer || !b->unbindWaveBuffer)) return STATUS_INVALID_PARAMETER;
#endif
#endif
    void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(Miniport),AUDIO_TAG);if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
    *out=new(mem) Miniport(b,nominal);return STATUS_SUCCESS;
}
NTSTATUS Mt8167NewMiniportWithBackend(PMINIPORTWAVERT *out,const Mt8167StreamBackend *b) { return NewMiniport(out,b,false); }
NTSTATUS Mt8167NewSpeakerMiniportWithBackend(PMINIPORTWAVERT *out,const Mt8167StreamBackend *b) {
    if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;if(!b)return STATUS_INVALID_PARAMETER;
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    if(b->capture)return STATUS_INVALID_PARAMETER;
#endif
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
    if(b->prepareRenderStream)return STATUS_INVALID_PARAMETER;
#endif
    return NewMiniport(out,b,true);
}
NTSTATUS Mt8167NewMiniport(PMINIPORTWAVERT *out) { return NewMiniport(out,NULL,false); }
#endif
