#include "pnp_backend.h"
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
#include "capture_owner.h"
#include "../../bus/mt8167-shared/capture_interface.h"
extern "C" NTSTATUS Mt8167QueryCaptureParent(PDEVICE_OBJECT,A04_CAPTURE_INTERFACE *);
#endif
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
#include "common_dma.h"
#include "dma_adapter.h"
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#include "wavert_dma.h"
#include "wavert_cache.h"
#endif
#include "stream_transport.h"
#if defined(A04_AUDIO_PCM_PATTERN_DIAGNOSTIC) && A04_AUDIO_PCM_PATTERN_DIAGNOSTIC
#if !defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) || !A04_AUDIO_SILENT_DMA_DIAGNOSTIC
#error PCM pattern requires the isolated DMA diagnostic path
#endif
#include "pcm_pattern.h"
#endif
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC && (!defined(A04_AUDIO_MAP_AFE) || !A04_AUDIO_MAP_AFE || !defined(A04_AUDIO_QUERY_SHARED) || !A04_AUDIO_QUERY_SHARED)
#error A04_AUDIO_IRQ_DIAGNOSTIC requires AFE mapping and parent query
#endif
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
#if !defined(A04_AUDIO_QUERY_SHARED) || !A04_AUDIO_QUERY_SHARED
#error A04_AUDIO_MAP_AFE requires the shared parent query
#endif
#include "afe_mapping.h"
#endif
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
#include "audio_parent_query.h"
#endif
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC && (!defined(A04_AUDIO_IRQ_DIAGNOSTIC) || !A04_AUDIO_IRQ_DIAGNOSTIC)
#error A04_AUDIO_RUN_DIAGNOSTIC requires the masked IRQ diagnostic path
#endif
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
#define A04_TRACE(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,__VA_ARGS__)
#else
#define A04_TRACE(...) ((void)0)
#endif
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC && (!defined(A04_AUDIO_RUN_DIAGNOSTIC) || !A04_AUDIO_RUN_DIAGNOSTIC)
#error silent DMA diagnostic requires RUN diagnostic
#endif
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
/* Link-time pairing prevents publishing the WaveRT buffer as silent DMA. */
extern "C" int Mt8167SilentDmaMiniportEnabled(void);
#endif
#if defined(A04_AUDIO_UNMUTE_DIAGNOSTIC) && A04_AUDIO_UNMUTE_DIAGNOSTIC && (!defined(A04_AUDIO_PCM_PATTERN_DIAGNOSTIC) || !A04_AUDIO_PCM_PATTERN_DIAGNOSTIC)
#error unmute diagnostic requires the isolated fixed PCM pattern
#endif
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
#if !defined(A04_AUDIO_RUN_DIAGNOSTIC) || !A04_AUDIO_RUN_DIAGNOSTIC || (defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC)
#error voice PCM requires RUN diagnostic and the direct WaveRT buffer
#endif
#endif
#if defined(A04_AUDIO_WAVE_DMA_PROBE) && A04_AUDIO_WAVE_DMA_PROBE
#if !defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) || !A04_AUDIO_VOICE_PCM_DIAGNOSTIC
#error wave DMA probe requires voice PCM diagnostic
#endif
#include "wave_dma_probe.h"
#endif
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED
#if !defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) || !A04_AUDIO_VOICE_PCM_DIAGNOSTIC || (defined(A04_AUDIO_WAVE_DMA_PROBE) && A04_AUDIO_WAVE_DMA_PROBE)
#error retained wave DMA requires voice PCM and excludes the mapping probe
#endif
#include "wave_dma_live.h"
#endif
#if defined(A04_AUDIO_DIGITAL_PLAYBACK) && A04_AUDIO_DIGITAL_PLAYBACK && (!defined(A04_AUDIO_RUN_DIAGNOSTIC) || !A04_AUDIO_RUN_DIAGNOSTIC)
#error digital playback requires the gated RUN path
#endif
#if defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK
#if !defined(A04_AUDIO_DIGITAL_PLAYBACK) || !A04_AUDIO_DIGITAL_PLAYBACK || !defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) || !A04_AUDIO_SILENT_DMA_DIAGNOSTIC || !defined(A04_AUDIO_PCM_PATTERN_DIAGNOSTIC) || !A04_AUDIO_PCM_PATTERN_DIAGNOSTIC || !defined(A04_AUDIO_UNMUTE_DIAGNOSTIC) || !A04_AUDIO_UNMUTE_DIAGNOSTIC
#error first playback requires digital playback, immutable private PCM and minimum unmute
#endif
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
#error first playback must not DMA a client-modifiable WaveRT buffer
#endif
#endif
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#if !A04_AUDIO_VOICE_PCM_DIAGNOSTIC || !A04_AUDIO_WAVE_DMA_RETAINED || !A04_AUDIO_DIGITAL_PLAYBACK || A04_AUDIO_FIRST_PLAYBACK || A04_AUDIO_SILENT_DMA_DIAGNOSTIC || A04_AUDIO_PCM_PATTERN_DIAGNOSTIC
#error engine PCM requires retained direct WaveRT DMA and excludes private tone
#endif
#endif
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
#include "headphone_endpoint.h"
#include "../../bus/mt8167-shared/headphone_interface.h"
#endif
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
#if !defined(A04_AUDIO_ENGINE_PCM) || !A04_AUDIO_ENGINE_PCM
#error DMA notification requires the direct Engine PCM lifecycle
#endif
#include "dma_notification.h"
#endif
#define OWNER_TAG 'OwnA'
struct Mt8167PnpBackend {
    Mt8167StreamBackend iface;
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
    A04_HEADPHONE_INTERFACE hpParent;Mt8167HeadphoneProvider hpProvider;
    BOOLEAN hpSelected,hpPinned,hpAnalogStarted;
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    A04_CAPTURE_INTERFACE captureParent;
#endif
    volatile LONG refs;
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
    Mt8167DmaNotifications notifications;
    ULONG notificationCount;
    PPORTWAVERTSTREAM notificationClient;
#endif
    KMUTEX mutex;
    EX_RUNDOWN_REF positions;
    EX_RUNDOWN_REF livePositions;
    BOOLEAN livePositionsClosed;
    BOOLEAN online;
    BOOLEAN digitalRetained;
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    ULONG diagSequence,diagStage,diagFailedStage;NTSTATUS diagStatus;
#endif
    PDEVICE_OBJECT pdo;
    PDMA_ADAPTER adapter;
    Mt8167CommonDma buffer;
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
    PMDL waveMdl; PVOID waveMapping; ULONG waveBytes; BOOLEAN waveProbePending;
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED
    Mt8167WaveDma waveDma; BOOLEAN waveReleased;
#endif
#endif
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
    Mt8167CommonDma silentBuffer;
#endif
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    Mt8167WaveBuffer retainedBuffer;
    PPORTWAVERTSTREAM engineClient;
    BOOLEAN resetOnRun;
    volatile LONG enginePoison;
    volatile LONG positionPaused,stoppedOffset;
    KSSTATE engineState;
#endif
    Mt8167PnpInterrupt interrupt;
    Mt8167Irq irq;
    volatile LONG irqEvents,irqFaults;
    struct mt8167_dl1_engine engine;
    Mt8167Transport transport;
    struct mt8167_audio_shared_client shared;
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
    Mt8167AfeMapping afe;
    BOOLEAN afeBound;
#endif
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
    struct Mt8167AudioParentClient parentClient;
#endif
};
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
/* Only the DL1 registers used by the engine are reachable. The PnP owner
 * keeps the mapping alive through interrupt disconnect and DPC rundown.
 * Callers serialize engine access with the IRQ synchronization lock. */
static bool OwnerDl1Offset(uint32_t offset, bool write)
{
    switch (offset) {
    case 0x10: case 0x14: case 0x40: case 0x48:
    case 0x3a0: case 0x3a8: case 0x3ac: case 0x3d8: return true;
    case 0x44: case 0x3a4: return !write;
    default: return false;
    }
}
static int OwnerDl1Read(void *context,uint32_t offset,uint32_t *value)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(context);
    if (!o || !o->afe.base || !value || !OwnerDl1Offset(offset,false)) return -1;
    *value=READ_REGISTER_ULONG(reinterpret_cast<volatile ULONG *>(
        static_cast<unsigned char *>(o->afe.base)+offset));return 0;
}
static int OwnerDl1Write(void *context,uint32_t offset,uint32_t value)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(context);
    if (!o || !o->afe.base || !OwnerDl1Offset(offset,true)) return -1;
    WRITE_REGISTER_ULONG(reinterpret_cast<volatile ULONG *>(
        static_cast<unsigned char *>(o->afe.base)+offset),value);return 0;
}
#endif
/* PnP, stream state and buffer operations run at PASSIVE_LEVEL. A kernel
 * mutex serializes them without the IRQL elevation of FAST_MUTEX: the DMA
 * adapter's AllocateCommonBuffer and FreeCommonBuffer require PASSIVE_LEVEL. */
static void OwnerLock(Mt8167PnpBackend *o)
{ (void)KeWaitForSingleObject(&o->mutex,Executive,KernelMode,FALSE,NULL); }
static void OwnerUnlock(Mt8167PnpBackend *o) { (void)KeReleaseMutex(&o->mutex,FALSE); }
static void OwnerReference(void *ctx) { Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx); InterlockedIncrement(&o->refs); }
static void OwnerDereference(void *ctx)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if (InterlockedDecrement(&o->refs)) return;
    /* A violated caller IRQL contract cannot safely free a DMA adapter. */
    if (KeGetCurrentIrql()!=PASSIVE_LEVEL) return;
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
    if (o->waveMdl) return;
#endif
    /* An uncertain lease release keeps the mapping and parent alive. */
    if (o->digitalRetained || o->shared.leased || o->shared.quarantined || o->irq.interrupt) return;
    /* RUN is always refused, so all buffers allocated here remain unexposed. */
    if (o->buffer.mdl && !NT_SUCCESS(Mt8167CommonReleaseUnexposed(&o->buffer))) {
        /* Never release DMA adapter/PDO when buffer lifetime is uncertain. */
        return;
    }
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
    if (o->silentBuffer.mdl && !NT_SUCCESS(Mt8167CommonReleaseUnexposed(&o->silentBuffer))) return;
#endif
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
    if (o->afeBound) {
        if (o->parentClient.parent.BindAfe(o->parentClient.parent.Header.Context,NULL,0)) return;
        o->afeBound=FALSE;
    }
    Mt8167UnmapAssignedAfe(&o->afe);
#endif
    if (o->shared.referenced && mt8167_audio_shared_detach(&o->shared)) return;
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
    if (o->parentClient.bound && Mt8167AudioParentClose(&o->parentClient)) return;
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    if(o->captureParent.Header.InterfaceDereference)
        o->captureParent.Header.InterfaceDereference(o->captureParent.Header.Context);
#endif
    o->adapter->DmaOperations->PutDmaAdapter(o->adapter);
    ObDereferenceObject(o->pdo);
    #if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
    if(o->hpParent.Header.InterfaceDereference)o->hpParent.Header.InterfaceDereference(o->hpParent.Header.Context);
#endif
    ExFreePoolWithTag(o,OWNER_TAG);
}
static NTSTATUS OwnerAllocate(void *ctx,ULONG bytes,PMDL *mdl,PULONG actual,PULONG offset,MEMORY_CACHING_TYPE *cache)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    NTSTATUS status;
    if (!mdl || !actual || !offset || !cache) return STATUS_INVALID_PARAMETER;
    *mdl=NULL;*actual=0;*offset=0;
    if (KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_DEVICE_NOT_READY;
    OwnerLock(o);
    if (!o->online) status=STATUS_DEVICE_NOT_READY;
    else if (o->buffer.mdl) status=STATUS_DEVICE_BUSY;
    else {
        status=Mt8167CommonAllocate(o->adapter,bytes,&o->buffer);
        if (NT_SUCCESS(status)) { *mdl=o->buffer.mdl;*actual=o->buffer.size;*cache=MmNonCached; }
    }
    OwnerUnlock(o);
    return status;
}
static void OwnerFree(void *ctx,PMDL mdl,ULONG bytes)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if (KeGetCurrentIrql()!=PASSIVE_LEVEL) return;
    OwnerLock(o);
    if (mdl && mdl==o->buffer.mdl && bytes==o->buffer.size)
        (void)Mt8167CommonReleaseUnexposed(&o->buffer);
    OwnerUnlock(o);
}
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
/* This records a PortCls allocation only. A physical page address is not a
 * DMA logical address; no mapping is published by this operation. */
static NTSTATUS OwnerBindWaveLocked(Mt8167PnpBackend *o,PMDL mdl,PVOID mapping,ULONG bytes) {
    NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if(!o || !mdl || !mapping || !bytes || bytes>1048576 || (bytes & (PAGE_SIZE-1)) ||
       MmGetMdlByteOffset(mdl) || MmGetMdlByteCount(mdl)<bytes || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(o->online && !o->waveMdl && !o->shared.quarantined) {
        o->waveMdl=mdl;o->waveMapping=mapping;o->waveBytes=bytes;result=STATUS_SUCCESS;
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED
        if(!o->shared.leased || !o->parentClient.parent.VerifyLivePcm ||
           !o->parentClient.parent.VerifyDmaDrained || !o->parentClient.parent.StartVoice ||
           !o->parentClient.parent.StopVoice) result=STATUS_DEVICE_NOT_READY;
        else {
            result=Mt8167WaveDmaMap(o->adapter,o->pdo,mdl,bytes,&o->waveDma);
            if(NT_SUCCESS(result) && o->parentClient.parent.VerifyLivePcm(
               o->parentClient.parent.Header.Context,mdl,(uint64_t)o->waveDma.logical.QuadPart,bytes)) {
                result=NT_SUCCESS(Mt8167WaveDmaRelease(&o->waveDma,FALSE)) ? STATUS_DEVICE_NOT_READY : STATUS_IO_TIMEOUT;
            }
            if(NT_SUCCESS(result)) {
                o->buffer.adapter=o->adapter;o->buffer.cpu=mapping;o->buffer.mdl=mdl;
                o->buffer.logical=o->waveDma.logical;o->buffer.size=bytes;
                o->buffer.prepared=FALSE;o->buffer.exposed=FALSE;o->waveReleased=FALSE;
            }
        }
        if(result==STATUS_IO_TIMEOUT) {
            o->waveProbePending=TRUE;
            if(!o->shared.quarantined)(void)mt8167_audio_shared_quarantine(&o->shared);
        } else if(!NT_SUCCESS(result)) {o->waveMdl=NULL;o->waveMapping=NULL;o->waveBytes=0;}
#endif
#if defined(A04_AUDIO_WAVE_DMA_PROBE) && A04_AUDIO_WAVE_DMA_PROBE
        result=Mt8167ProbeWaveDma(o->adapter,o->pdo,mdl,bytes);
        if(result==STATUS_IO_TIMEOUT) {
            /* An outstanding adapter callback still owns this MDL. Do not
             * unbind or release the parent, even after PnP revoke. */
            o->waveProbePending=TRUE;
            if(!o->shared.quarantined)(void)mt8167_audio_shared_quarantine(&o->shared);
        } else if(!NT_SUCCESS(result)) {o->waveMdl=NULL;o->waveMapping=NULL;o->waveBytes=0;}
#endif
    }
    return result;
}
static NTSTATUS OwnerBindWave(void *ctx,PMDL mdl,PVOID mapping,ULONG bytes) {
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if(!o)return STATUS_INVALID_PARAMETER;
    OwnerLock(o);NTSTATUS result=OwnerBindWaveLocked(o,mdl,mapping,bytes);OwnerUnlock(o);return result;
}
static NTSTATUS OwnerUnbindWave(void *ctx,PMDL mdl) {
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if(!o || !mdl || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    OwnerLock(o);
    if(o->waveMdl==mdl && !o->waveProbePending && o->transport.state!=AUDIO_RUNNING) {
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED
        if(o->waveReleased || (!o->buffer.exposed && o->waveDma.list &&
           NT_SUCCESS(Mt8167WaveDmaRelease(&o->waveDma,FALSE)))) {
            RtlZeroMemory(&o->buffer,sizeof(o->buffer));
            o->waveMdl=NULL;o->waveMapping=NULL;o->waveBytes=0;result=STATUS_SUCCESS;
        }
#else
        if(!o->shared.quarantined) {o->waveMdl=NULL;o->waveMapping=NULL;o->waveBytes=0;result=STATUS_SUCCESS;}
#endif
    }
    OwnerUnlock(o);return result;
}
#endif
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
static NTSTATUS OwnerConnectMaskedIrq(Mt8167PnpBackend *);
#endif
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
static NTSTATUS HpCalibrateLocked(Mt8167PnpBackend*);
#endif
static NTSTATUS OwnerCheckoutRoute(void *ctx,PPORTWAVERTSTREAM port,ULONG bytes,PMDL *mdl,PULONG actual,BOOLEAN headphone)
{
#if !defined(A04_AUDIO_HP_SHARED) || !A04_AUDIO_HP_SHARED
    UNREFERENCED_PARAMETER(headphone);
#endif
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if(!o || !port || !mdl || !actual || !bytes || bytes>1048576 ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    *mdl=NULL;*actual=0;OwnerLock(o);NTSTATUS result=STATUS_DEVICE_NOT_READY;
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
    if(o->notificationClient && o->notificationClient!=port){OwnerUnlock(o);return STATUS_DEVICE_BUSY;}
#endif
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
    /* The first exposed route pins the single engine until certified drain.
     * Reject BEFORE IRQ connect, allocation, binding, zeroing or selection. */
    if((o->digitalRetained || o->hpPinned) && o->hpSelected!=headphone) {
        OwnerUnlock(o);return STATUS_DEVICE_BUSY;
    }
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    if(o->online&&!o->shared.leased){result=OwnerConnectMaskedIrq(o);if(!NT_SUCCESS(result)){OwnerUnlock(o);return result;}}
#endif
    if(o->online && !o->enginePoison && !o->waveProbePending &&
       !o->engineClient && (o->transport.state==AUDIO_READY ||
        o->transport.state==AUDIO_STOPPED_RETAINED)) {
        if(!o->retainedBuffer.mdl) {
            result=Mt8167AllocateBuffer(port,bytes,&o->retainedBuffer);
            if(NT_SUCCESS(result)) {
                result=OwnerBindWaveLocked(o,o->retainedBuffer.mdl,
                    o->retainedBuffer.mapping,o->retainedBuffer.bytes);
                if(!NT_SUCCESS(result)) {
                    if(result==STATUS_IO_TIMEOUT)InterlockedExchange(&o->enginePoison,1);
                    else (void)Mt8167ReleaseUnpublishedBuffer(&o->retainedBuffer);
                }
            }
        } else {
            result=bytes<=o->retainedBuffer.bytes ? Mt8167VerifyWaveCache(
                o->retainedBuffer.mapping,o->retainedBuffer.bytes,o->retainedBuffer.physicalAddress) :
                STATUS_BUFFER_TOO_SMALL;
            if(result==STATUS_DEVICE_CONFIGURATION_ERROR)
                InterlockedExchange(&o->enginePoison,1);
        }
        if(NT_SUCCESS(result)) {
            /* PortCls retires its client mapping on FreeAudioBuffer. The
             * allocating port reference and MDL remain in this owner. */
            RtlZeroMemory(o->retainedBuffer.mapping,o->retainedBuffer.bytes);
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
            if(headphone){
                o->hpSelected=TRUE;
                result=HpCalibrateLocked(o);
                if(!NT_SUCCESS(result)){OwnerUnlock(o);return result;}
            }
#endif
            o->engineClient=port;o->engineState=KSSTATE_STOP;
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
            o->hpSelected=headphone;
#endif
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
            o->resetOnRun=headphone?FALSE:TRUE;
#else
            o->resetOnRun=TRUE;
#endif
            InterlockedExchange(&o->stoppedOffset,0);InterlockedExchange(&o->positionPaused,1);
            *mdl=o->retainedBuffer.mdl;*actual=o->retainedBuffer.bytes;
        }
    }
    OwnerUnlock(o);return result;
}
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
static NTSTATUS OwnerNotificationCount(void *ctx,PPORTWAVERTSTREAM port,ULONG count)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if(!o || !port || (count!=1 && count!=2) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    OwnerLock(o);NTSTATUS result=STATUS_DEVICE_NOT_READY;
    struct mt8167_dl1_plan plan;
    if(o->online && !o->enginePoison && o->engineClient==port &&
       o->engineState==KSSTATE_STOP && o->retainedBuffer.mdl &&
       mt8167_dl1_plan((uint64_t)o->waveDma.logical.QuadPart,o->retainedBuffer.bytes,
           (count==1?o->retainedBuffer.bytes:o->retainedBuffer.bytes/2),48000,&plan)) {
        o->notificationCount=count;result=STATUS_SUCCESS;
    }
    OwnerUnlock(o);return result;
}
static NTSTATUS OwnerNotificationEvent(void *ctx,PPORTWAVERTSTREAM port,PKEVENT event,BOOLEAN add)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if(!o || !port || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    OwnerLock(o);NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if(o->online && (!o->engineClient || o->engineClient==port) &&
       (!o->notificationClient || o->notificationClient==port)) {
        result=add?Mt8167NotificationsRegister(&o->notifications,event):
            Mt8167NotificationsUnregister(&o->notifications,event);
        if(NT_SUCCESS(result) && add)o->notificationClient=port;
    }
    OwnerUnlock(o);return result;
}
static void OwnerClearNotifications(void *ctx,PPORTWAVERTSTREAM port)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if(!o || !port || KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
    OwnerLock(o);
    if(o->notificationClient==port || o->engineClient==port) {
        Mt8167NotificationsClear(&o->notifications);o->notificationCount=0;o->notificationClient=NULL;
    }
    OwnerUnlock(o);
}
static ULONG OwnerPeriodBytes(Mt8167PnpBackend *o)
{return o->notificationCount==1?o->buffer.size:o->buffer.size/2;}
#endif
static NTSTATUS OwnerCheckout(void *ctx,PPORTWAVERTSTREAM port,ULONG bytes,PMDL *mdl,PULONG actual)
{return OwnerCheckoutRoute(ctx,port,bytes,mdl,actual,FALSE);}
static NTSTATUS OwnerCheckin(void *ctx,PPORTWAVERTSTREAM port,PMDL mdl)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if(!o || !port || !mdl || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    OwnerLock(o);NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if(o->engineClient==port && mdl==o->retainedBuffer.mdl &&
       o->transport.state!=AUDIO_RUNNING) {
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
        Mt8167NotificationsClear(&o->notifications);o->notificationCount=0;o->notificationClient=NULL;
#endif
        o->engineClient=NULL;o->engineState=KSSTATE_STOP;o->resetOnRun=TRUE;
        result=STATUS_SUCCESS; /* ownership only; pages/SG/port stay pinned */
    }
    OwnerUnlock(o);return result;
}
#endif
/* Direct engine PCM uses retained identity DMA of WC/Normal-NC PortCls pages.
 * A once-only flush alone never admits a client-modifiable cyclic buffer. */
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
static BOOLEAN OwnerLiveDmaVerified(Mt8167PnpBackend *o)
{
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
    /* A private zero-filled common buffer is never exposed to WaveRT. The
     * transport flushes it once before publishing its bus address. */
    if(!o || !o->buffer.mdl || o->silentBuffer.mdl || !Mt8167SilentDmaMiniportEnabled())return FALSE;
    if(!NT_SUCCESS(Mt8167CommonAllocate(o->adapter,o->buffer.size,&o->silentBuffer)))return FALSE;
    /* The two active DMA allocations must not overlap in either address space. */
    if(o->silentBuffer.cpu==o->buffer.cpu ||
       ((ULONGLONG)o->silentBuffer.logical.QuadPart < (ULONGLONG)o->buffer.logical.QuadPart+o->buffer.size &&
        (ULONGLONG)o->buffer.logical.QuadPart < (ULONGLONG)o->silentBuffer.logical.QuadPart+o->silentBuffer.size))return FALSE;
#if defined(A04_AUDIO_PCM_PATTERN_DIAGNOSTIC) && A04_AUDIO_PCM_PATTERN_DIAGNOSTIC
    if(mt8167_pcm_pattern(o->silentBuffer.cpu,o->silentBuffer.size))return FALSE;
#endif
    o->transport.dma=&o->silentBuffer;
    return TRUE;
#elif defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED
    if(!o || !o->waveDma.list || !o->waveMdl || o->waveDma.exposed ||
       !o->parentClient.parent.VerifyLivePcm || o->parentClient.parent.VerifyLivePcm(
       o->parentClient.parent.Header.Context,o->waveMdl,(uint64_t)o->waveDma.logical.QuadPart,o->waveBytes))return FALSE;
    o->transport.dma=&o->buffer;return TRUE;
#elif defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
    /* PortCls pages require a DMA adapter mapping with a device logical
     * address and retained map registers. That mapping is not implemented. */
    UNREFERENCED_PARAMETER(o);return FALSE;
#else
    UNREFERENCED_PARAMETER(o);return FALSE;
#endif
}
#endif
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
static NTSTATUS OwnerStopRunning(Mt8167PnpBackend *o);
#endif
static int OwnerRouteStart(Mt8167PnpBackend *o){
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
 if(o->hpSelected){if(o->hpAnalogStarted)return 0;NTSTATUS s=o->hpParent.Start(o->hpParent.Header.Context);if(NT_SUCCESS(s)){o->hpAnalogStarted=TRUE;o->hpPinned=TRUE;}return NT_SUCCESS(s)?0:-1;}
#endif
 return o->parentClient.parent.StartVoice(o->parentClient.parent.Header.Context);
}
static int OwnerRouteMute(Mt8167PnpBackend *o){
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
 if(o->hpSelected){if(!o->hpAnalogStarted&&OwnerRouteStart(o))return -1;return NT_SUCCESS(o->hpParent.MuteRetained(o->hpParent.Header.Context))?0:-1;}
#endif
 return o->parentClient.parent.Mute(o->parentClient.parent.Header.Context);
}
static int OwnerRouteUnmute(Mt8167PnpBackend *o){
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
 if(o->hpSelected)return NT_SUCCESS(o->hpParent.ResumeRetained(o->hpParent.Header.Context))?0:-1;
#endif
 return o->parentClient.parent.UnmuteMin(o->parentClient.parent.Header.Context);
}
static NTSTATUS OwnerStartHardwareLocked(void *ctx,BOOLEAN *possiblyExposed) {
    if (!ctx || !possiblyExposed) return STATUS_INVALID_PARAMETER;
    *possiblyExposed=FALSE;
    if (KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_DEVICE_NOT_READY;
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    NTSTATUS result=STATUS_DEVICE_NOT_READY;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    if(o->enginePoison) {return result;}
    if(o->online && o->engineClient && o->shared.leased && o->shared.quarantined &&
       o->digitalRetained && o->transport.state==AUDIO_STOPPED_RETAINED) {
        *possiblyExposed=TRUE;o->diagStage=A04_STAGE_DMA;
        if(!OwnerRouteMute(o) &&
           !Mt8167TransportRestartRetained(&o->transport,o->resetOnRun)) {
            o->diagStage=A04_STAGE_UNMUTE;
            if(!OwnerRouteUnmute(o)) {
                result=STATUS_SUCCESS;
                if(o->livePositionsClosed) {
                    ExReInitializeRundownProtection(&o->livePositions);o->livePositionsClosed=FALSE;
                }
                InterlockedExchange(&o->positionPaused,0);
            } else {
                o->diagFailedStage=A04_STAGE_UNMUTE;(void)OwnerStopRunning(o);
            }
        }
        if(!NT_SUCCESS(result))InterlockedExchange(&o->enginePoison,1);
        o->diagStatus=result;if(NT_SUCCESS(result))o->diagStage=A04_STAGE_RUNNING;
        else if(!o->diagFailedStage)o->diagFailedStage=o->diagStage;
        A04_TRACE("A04AUD retained restart result=%08lx reset=%d generation=%ld poison=%d; bus-drain=unknown\n",
            (ULONG)result,o->resetOnRun,o->irq.generation,o->enginePoison);
        return result;
    }
#endif
    if (o->online && o->buffer.mdl && o->shared.leased && !o->shared.quarantined &&
        !o->digitalRetained && o->transport.state==AUDIO_READY && o->irq.interrupt && OwnerLiveDmaVerified(o)) {
#if defined(A04_AUDIO_DIGITAL_PLAYBACK) && A04_AUDIO_DIGITAL_PLAYBACK
        /* The parent owns clocks, routing and codec hw_params. Retain the
         * lease after even partial writes; never infer quiescence from DL1. */
        if(!o->parentClient.parent.PreparePlayback) {return result;}
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
        o->diagStage=A04_STAGE_DIGITAL;o->diagStatus=STATUS_DEVICE_NOT_READY;
#endif
        int digital=o->parentClient.parent.PreparePlayback(o->parentClient.parent.Header.Context,48000);
        if(digital!= -1) {o->digitalRetained=TRUE;*possiblyExposed=TRUE;}
        if(digital) {
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
            o->diagFailedStage=o->diagStage;
#endif
            if(o->digitalRetained)(void)mt8167_audio_shared_quarantine(&o->shared);
            return result;
        }
#endif
#if defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK
        /* The private common buffer has fixed PCM, CacheEnabled=FALSE and
         * no user mapping. PrepareForDevice flushes once before DL1 starts;
         * no CPU writes occur afterward. No cyclic-client coherency claim. */
        o->diagStage=A04_STAGE_VOICE;
        if(!o->parentClient.parent.StartVoice ||
           OwnerRouteStart(o)) {
            (void)mt8167_audio_shared_quarantine(&o->shared);
            o->diagFailedStage=o->diagStage;
            *possiblyExposed=TRUE;return result;
        }
#endif
        /* Period is half-ring for polling, or the requested one/two DMA
         * notification boundaries for the standard event-driven stream. */
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
        /* The owner mutex serializes PnP revoke. Quarantine on every
         * attempted voice operation, including a failed DMA preparation. */
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
        o->diagStage=A04_STAGE_VOICE;
#endif
        if (!OwnerRouteStart(o)) {
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED
            /* Exposure precedes any synchronized DL1 register writes. */
            o->waveDma.exposed=TRUE;
#endif
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
            o->diagStage=A04_STAGE_DMA;
#endif
            if (!Mt8167TransportStartMaskedParent(&o->transport,
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
                OwnerPeriodBytes(o),&o->shared
#else
                o->buffer.size/2,&o->shared
#endif
                )) {
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
                o->diagStage=A04_STAGE_UNMUTE;
#endif
                if(OwnerRouteUnmute(o)) {
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
                    o->diagFailedStage=A04_STAGE_UNMUTE;
#endif
                    (void)OwnerStopRunning(o);
                } else result=STATUS_SUCCESS;
            }
        }
        if (!o->shared.quarantined) (void)mt8167_audio_shared_quarantine(&o->shared);
#else
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
        o->diagStage=A04_STAGE_DMA;
#endif
        if (!Mt8167TransportStartMaskedParent(&o->transport,
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
                OwnerPeriodBytes(o),&o->shared
#else
                o->buffer.size/2,&o->shared
#endif
                )) {
#if defined(A04_AUDIO_UNMUTE_DIAGNOSTIC) && A04_AUDIO_UNMUTE_DIAGNOSTIC
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
            o->diagStage=A04_STAGE_UNMUTE;
#endif
            /* Failed/ambiguous unmute still stops DMA and retains the lease. */
            if(OwnerRouteUnmute(o)) {
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
                o->diagFailedStage=A04_STAGE_UNMUTE;
#endif
                (void)OwnerStopRunning(o);
            } else
#endif
                result=STATUS_SUCCESS;
        }
#endif
        if(o->digitalRetained && !o->shared.quarantined)
            (void)mt8167_audio_shared_quarantine(&o->shared);
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
        *possiblyExposed=(o->silentBuffer.exposed || o->shared.quarantined) ? TRUE : FALSE;
#else
        *possiblyExposed=(o->buffer.exposed || o->shared.quarantined) ? TRUE : FALSE;
#endif
    }
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    o->diagStatus=result;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    if(*possiblyExposed)o->retainedBuffer.handedToDevice=TRUE;
    if(NT_SUCCESS(result))InterlockedExchange(&o->positionPaused,0);
    if(!NT_SUCCESS(result) && *possiblyExposed)InterlockedExchange(&o->enginePoison,1);
#endif
    if(NT_SUCCESS(result))o->diagStage=A04_STAGE_RUNNING;
    else if(!o->diagFailedStage)o->diagFailedStage=o->diagStage;
#endif
    A04_TRACE("A04AUD run result=%08lx online=%d buffer=%d lease=%d irq=%d live-dma=selected-mode exposed=%d\n",(ULONG)result,o->online,o->buffer.mdl!=NULL,o->shared.leased,o->irq.interrupt!=NULL,*possiblyExposed);
    return result;
#else
    return STATUS_DEVICE_NOT_READY;
#endif
}
/* Called with the owner mutex held by either legacy start or SetState. */
static NTSTATUS OwnerStartLocked(void *ctx,BOOLEAN *possiblyExposed)
{
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    *possiblyExposed=FALSE;
    if(o->transport.state==AUDIO_STOPPED_RETAINED) {
        struct mt8167_dl1_plan plan;
        if(!mt8167_dl1_plan((uint64_t)o->buffer.logical.QuadPart,o->buffer.size,
            OwnerPeriodBytes(o),48000,&plan))return STATUS_INVALID_PARAMETER;
        o->transport.plan=plan;
    }
    Mt8167NotificationsEnable(&o->notifications,o->notificationCount!=0);
#endif
    NTSTATUS result=OwnerStartHardwareLocked(ctx,possiblyExposed);
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
    if(!NT_SUCCESS(result))Mt8167NotificationsEnable(&o->notifications,FALSE);
#endif
    return result;
}
static NTSTATUS OwnerStart(void *ctx,BOOLEAN *possiblyExposed) {
    if(!ctx || !possiblyExposed)return STATUS_INVALID_PARAMETER;
    *possiblyExposed=FALSE;
    if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_DEVICE_NOT_READY;
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    OwnerLock(o);NTSTATUS result=OwnerStartLocked(ctx,possiblyExposed);OwnerUnlock(o);return result;
}

#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
/* Caller holds the passive-level backend mutex. Stop DL1 even if mute fails,
 * retaining resources on error. */
static NTSTATUS OwnerStopRunning(Mt8167PnpBackend *o)
{
    /* Close position admission and wait for readers before stopping IRQ.
     * Engine PCM reopens this rundown only after a verified retained restart. */
    if(!o->livePositionsClosed) {
        ExWaitForRundownProtectionRelease(&o->livePositions);
        o->livePositionsClosed=TRUE;
    }
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    o->diagStage=A04_STAGE_STOP;
#endif
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
    Mt8167NotificationsEnable(&o->notifications,FALSE);
#endif
    int mute=OwnerRouteMute(o);
    int stop=Mt8167TransportStop(&o->transport);
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
    if(mute || stop || !o->parentClient.parent.StopVoice) {
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
        InterlockedExchange(&o->enginePoison,1);
#endif
        return STATUS_DEVICE_NOT_READY;
    }
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
    if(o->hpSelected){ULONG drain=A04_HP_DRAIN_UNKNOWN;
      NTSTATUS hp=o->hpParent.Stop(o->hpParent.Header.Context,&drain);
      if(!NT_SUCCESS(hp)||(drain!=A04_HP_DRAIN_RETAINED&&drain!=A04_HP_DRAIN_RELEASED)){InterlockedExchange(&o->enginePoison,1);return STATUS_DEVICE_NOT_READY;}
      if(drain==A04_HP_DRAIN_RELEASED)o->hpAnalogStarted=FALSE;
      /* Both results retain the existing pages/SG/D0; only RELEASED certifies
       * hardware drain. RETAINED never permits freeing or endpoint switching. */
    }
#endif
    /* Software IRQ work drained and DL1 disabled. Same pages/SG/voice/D0
     * stay pinned; never use this as a DMA bus-drain/free certificate. */
    uint32_t offset=0;
    /* Linux pcm_pointer maps a successfully read CUR==0 to buffer base.
     * A read failure already poisoned the synchronized stop. */
    if(o->irq.stoppedCurrent &&
       !mt8167_dl1_position(&o->transport.plan,o->irq.stoppedCurrent,&offset)) {
        InterlockedExchange(&o->enginePoison,1);return STATUS_DEVICE_NOT_READY;
    }
    InterlockedExchange(&o->stoppedOffset,(LONG)offset);
    InterlockedExchange(&o->positionPaused,1);
    o->diagStatus=STATUS_SUCCESS;o->diagStage=A04_STAGE_STOPPED_PINNED;
    return STATUS_SUCCESS;
#else
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED
    if(!o->parentClient.parent.VerifyDmaDrained ||
       o->parentClient.parent.VerifyDmaDrained(o->parentClient.parent.Header.Context))return STATUS_DEVICE_NOT_READY;
#endif
    if(o->parentClient.parent.StopVoice(o->parentClient.parent.Header.Context))return STATUS_DEVICE_NOT_READY;
#if defined(A04_AUDIO_WAVE_DMA_RETAINED) && A04_AUDIO_WAVE_DMA_RETAINED
    if(!NT_SUCCESS(Mt8167WaveDmaRelease(&o->waveDma,TRUE)))return STATUS_DEVICE_NOT_READY;
    RtlZeroMemory(&o->buffer,sizeof(o->buffer));o->waveReleased=TRUE;
#endif
    return STATUS_SUCCESS;
#endif
#else
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    o->diagStatus=!mute && !stop ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
    if(NT_SUCCESS(o->diagStatus))o->diagStage=A04_STAGE_STOPPED_PINNED;
#endif
    return !mute && !stop ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
#endif
}
#endif
static NTSTATUS OwnerStopLocked(void *ctx) {
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if (!o)return STATUS_INVALID_PARAMETER;
    if (o->transport.state==AUDIO_RUNNING)result=OwnerStopRunning(o);
    A04_TRACE("A04AUD stop result=%08lx state=%d quarantine=%d irq-events=%ld irq-faults=%ld; drain=unknown\n",(ULONG)result,(int)o->transport.state,o->shared.quarantined,o->irqEvents,o->irqFaults);
    return result;
#else
    UNREFERENCED_PARAMETER(ctx);return STATUS_DEVICE_NOT_READY;
#endif
}
static NTSTATUS OwnerStop(void *ctx) {
    if(!ctx || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_DEVICE_NOT_READY;
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    OwnerLock(o);NTSTATUS result=OwnerStopLocked(ctx);OwnerUnlock(o);return result;
}
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
static NTSTATUS OwnerSetState(void *ctx,PPORTWAVERTSTREAM port,KSSTATE state)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if(!o || !port || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
       (state!=KSSTATE_STOP && state!=KSSTATE_ACQUIRE && state!=KSSTATE_PAUSE &&
        state!=KSSTATE_RUN))return STATUS_INVALID_PARAMETER;
    /* Keep client/state admission atomic with PnP/checkin. Locked helpers
     * never reacquire the mutex; DPC notification never takes this mutex. */
    OwnerLock(o);NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if(o->online && !o->enginePoison && o->engineClient==port) {
        if(state==o->engineState)result=STATUS_SUCCESS;
        else if(state==KSSTATE_RUN) {
            BOOLEAN exposed=TRUE;result=OwnerStartLocked(o,&exposed);
            if(!NT_SUCCESS(result) && exposed)InterlockedExchange(&o->enginePoison,1);
        } else if(o->transport.state==AUDIO_RUNNING)result=OwnerStopLocked(o);
        else result=(o->transport.state==AUDIO_READY ||
                     o->transport.state==AUDIO_STOPPED_RETAINED) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
        if(NT_SUCCESS(result)) {
            o->engineState=state;if(state==KSSTATE_STOP) {
                o->resetOnRun=TRUE;InterlockedExchange(&o->stoppedOffset,0);
            }
            else if(state==KSSTATE_RUN)o->resetOnRun=FALSE;
        }
    }
    OwnerUnlock(o);return result;
}
#endif
static NTSTATUS OwnerPosition(void *ctx,PKSAUDIO_POSITION position)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    uint32_t offset;
    if (!o || !position) return STATUS_INVALID_PARAMETER;
    if (!ExAcquireRundownProtection(&o->positions)) return STATUS_DEVICE_NOT_READY;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    if(InterlockedCompareExchange(&o->enginePoison,0,0)) {
        ExReleaseRundownProtection(&o->positions);return STATUS_DEVICE_NOT_READY;
    }
    if(InterlockedCompareExchange(&o->positionPaused,0,0)) {
        position->PlayOffset=position->WriteOffset=(ULONG)InterlockedCompareExchange(&o->stoppedOffset,0,0);
        ExReleaseRundownProtection(&o->positions);return STATUS_SUCCESS;
    }
#endif
    /* No hardware path can set AUDIO_RUNNING yet. A future start must publish
     * the transport under a nonblocking synchronization scheme first. */
    NTSTATUS status=STATUS_DEVICE_NOT_READY;
    if (ExAcquireRundownProtection(&o->livePositions)) {
        status=Mt8167TransportPosition(&o->transport,&offset)==0 ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
        if (NT_SUCCESS(status)) { position->PlayOffset=offset;position->WriteOffset=offset; }
        ExReleaseRundownProtection(&o->livePositions);
    }
    ExReleaseRundownProtection(&o->positions);return status;
}
static NTSTATUS OwnerDescription(void *context,PDEVICE_DESCRIPTION description) {
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(context);
    if(!o || !description || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    OwnerLock(o);
    if(!o->online){OwnerUnlock(o);return STATUS_DEVICE_NOT_READY;}
    RtlZeroMemory(description,sizeof(*description));
    description->Version=DEVICE_DESCRIPTION_VERSION;description->Master=TRUE;
    description->Dma32BitAddresses=TRUE;description->InterfaceType=Internal;
    description->MaximumLength=1048576;
    OwnerUnlock(o);return STATUS_SUCCESS;
#else
    UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(description);return STATUS_DEVICE_NOT_READY;
#endif
}
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
static void OwnerIrqNotify(void *ctx,ULONG periods,BOOLEAN fault) {
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);LONG count;
    if(!o)return;
    count=InterlockedIncrement(&o->irqEvents);
    if(fault) {
        InterlockedIncrement(&o->irqFaults);
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
        InterlockedExchange(&o->enginePoison,1);
#endif
    }
    if(count<=4 || fault) A04_TRACE("A04AUD irq notification=%ld periods=%lu fault=%d\n",count,periods,fault);
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
    Mt8167NotificationsSignal(&o->notifications,periods,fault);
#else
    UNREFERENCED_PARAMETER(periods);
#endif
}
/* Caller serializes against revoke. The parent's v4 lease excludes competing
 * AFE/clock owners; other AFE sources remain masked through release. */
static NTSTATUS OwnerConnectMaskedIrq(Mt8167PnpBackend *o)
{
    NTSTATUS status;
    if (mt8167_audio_shared_acquire(&o->shared,48000)) {
        A04_TRACE("A04AUD irq lease rejected quarantine=%d\n",o->shared.quarantined);return STATUS_DEVICE_NOT_READY;
    }
    if (o->parentClient.parent.PrepareMaskedIrq(o->parentClient.parent.Header.Context,o->afe.base,0x1000)) {
        (void)mt8167_audio_shared_release_unexposed(&o->shared);return STATUS_DEVICE_NOT_READY;
    }
    status=Mt8167ConnectIrq(&o->irq,&o->engine,o->interrupt.vector,o->interrupt.level,
        o->interrupt.affinity,o->interrupt.mode,OwnerIrqNotify,o);
    if (!NT_SUCCESS(status)) (void)mt8167_audio_shared_release_unexposed(&o->shared);
    return status;
}
#endif
static NTSTATUS CreateBackend(PDEVICE_OBJECT pdo,PRESOURCELIST resources,Mt8167PnpBackend **out
#if A04_AUDIO_START_PERSIST
 ,Mt8167AudioStartRecord *record
#endif
 )
{
    ULONG mapRegisters=0;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR irq,rawIrq;
    Mt8167PnpBackend *o;
    NTSTATUS status;
    if (!pdo || !resources || !out) return STATUS_INVALID_PARAMETER;
    *out=NULL;
    if (resources->NumberOfEntriesOfType(CmResourceTypeInterrupt)!=1) return STATUS_DEVICE_CONFIGURATION_ERROR;
    irq=resources->FindTranslatedEntry(CmResourceTypeInterrupt,0);
    rawIrq=resources->FindUntranslatedEntry(CmResourceTypeInterrupt,0);
    if (!rawIrq || rawIrq->ShareDisposition!=CmResourceShareDeviceExclusive ||
        (rawIrq->Flags & (CM_RESOURCE_INTERRUPT_LATCHED | CM_RESOURCE_INTERRUPT_MESSAGE)) ||
        !rawIrq->u.Interrupt.Vector ||
        !irq || irq->ShareDisposition!=CmResourceShareDeviceExclusive ||
        (irq->Flags & (CM_RESOURCE_INTERRUPT_LATCHED | CM_RESOURCE_INTERRUPT_MESSAGE)) ||
        !irq->u.Interrupt.Vector || !irq->u.Interrupt.Affinity ||
        irq->u.Interrupt.Level > HIGH_LEVEL) return STATUS_DEVICE_CONFIGURATION_ERROR;
#if defined(NT_PROCESSOR_GROUPS)
    /* IoConnectInterrupt uses a group-zero KAFFINITY; never discard a group. */
    if (irq->u.Interrupt.Group != 0) return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif

#if A04_AUDIO_START_PERSIST
    if(record)record->stage=A04_START_OWNER_POOL;
#endif
    o=static_cast<Mt8167PnpBackend *>(ExAllocatePoolWithTag(NonPagedPool,sizeof(*o),OWNER_TAG));
    if (!o) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(o,sizeof(*o));
    KeInitializeMutex(&o->mutex,0);
    ExInitializeRundownProtection(&o->positions);
    ExInitializeRundownProtection(&o->livePositions);

#if A04_AUDIO_START_PERSIST
    if(record)record->stage=A04_START_DMA;
#endif
#if A04_AUDIO_START_PERSIST
    status=Mt8167GetDmaAdapterWithRecord(pdo,&o->adapter,&mapRegisters,record);
#else
    status=Mt8167GetDmaAdapter(pdo,&o->adapter,&mapRegisters);
#endif
    if (!NT_SUCCESS(status)) { ExFreePoolWithTag(o,OWNER_TAG);return status; }
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
    {
        A04_SHARED_AUDIO_INTERFACE queried;

#if A04_AUDIO_START_PERSIST
    if(record)record->stage=A04_START_PARENT_QUERY;
#endif
        status=Mt8167QueryAudioParent(pdo,&queried);
        if (!NT_SUCCESS(status)) { o->adapter->DmaOperations->PutDmaAdapter(o->adapter);ExFreePoolWithTag(o,OWNER_TAG);return status; }

#if A04_AUDIO_START_PERSIST
    if(record)record->stage=A04_START_PARENT_BIND;
#endif
        if (Mt8167AudioParentBind(&o->parentClient,&queried)) {
            if (queried.Header.InterfaceDereference) queried.Header.InterfaceDereference(queried.Header.Context);
            o->adapter->DmaOperations->PutDmaAdapter(o->adapter);ExFreePoolWithTag(o,OWNER_TAG);return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

#if A04_AUDIO_START_PERSIST
    if(record)record->stage=A04_START_SHARED_ATTACH;
#endif
        if (mt8167_audio_shared_attach(&o->shared,Mt8167AudioParentService(&o->parentClient))) {
            (void)Mt8167AudioParentClose(&o->parentClient);
            o->adapter->DmaOperations->PutDmaAdapter(o->adapter);ExFreePoolWithTag(o,OWNER_TAG);return STATUS_DEVICE_NOT_READY;
        }
    }
#endif
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE

#if A04_AUDIO_START_PERSIST
    if(record)record->stage=A04_START_AFE_MAP;
#endif
    status=Mt8167MapAssignedAfe(resources,&o->afe);
    if (!NT_SUCCESS(status)) {
        (void)mt8167_audio_shared_detach(&o->shared);
        (void)Mt8167AudioParentClose(&o->parentClient);
        o->adapter->DmaOperations->PutDmaAdapter(o->adapter);ExFreePoolWithTag(o,OWNER_TAG);return status;
    }
#endif
    ObReferenceObject(pdo);o->pdo=pdo;o->refs=1;o->online=TRUE;
    o->interrupt.vector=irq->u.Interrupt.Vector;
    o->interrupt.level=(KIRQL)irq->u.Interrupt.Level;
    o->interrupt.affinity=irq->u.Interrupt.Affinity;
    o->interrupt.mode=LevelSensitive;
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
    o->engine.io.context=o;o->engine.io.read32=OwnerDl1Read;o->engine.io.write32=OwnerDl1Write;
    o->transport.engine=&o->engine;
#endif
    o->transport.irq=&o->irq;o->transport.dma=&o->buffer;
    o->transport.vector=o->interrupt.vector;o->transport.level=o->interrupt.level;
    o->transport.affinity=o->interrupt.affinity;o->transport.mode=o->interrupt.mode;
    o->iface.context=o;o->iface.reference=OwnerReference;o->iface.dereference=OwnerDereference;
    o->iface.allocate=OwnerAllocate;o->iface.freeBuffer=OwnerFree;
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
    o->iface.bindWaveBuffer=OwnerBindWave;o->iface.unbindWaveBuffer=OwnerUnbindWave;
#endif
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
    Mt8167NotificationsInit(&o->notifications);
    o->iface.notificationCount=OwnerNotificationCount;
    o->iface.notificationEvent=OwnerNotificationEvent;
    o->iface.clearNotifications=OwnerClearNotifications;
#endif
    o->iface.checkout=OwnerCheckout;o->iface.checkin=OwnerCheckin;o->iface.setState=OwnerSetState;
#endif
    o->iface.start=OwnerStart;o->iface.stop=OwnerStop;o->iface.position=OwnerPosition;o->iface.getDeviceDescription=OwnerDescription;
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC

#if A04_AUDIO_START_PERSIST
    if(record)record->stage=A04_START_AFE_BIND;
#endif
    if (o->parentClient.parent.BindAfe(o->parentClient.parent.Header.Context,o->afe.base,0x1000)) {
        o->online=FALSE;OwnerDereference(o);return STATUS_DEVICE_NOT_READY;
    }
    o->afeBound=TRUE;
#if !defined(A04_AUDIO_CAPTURE) || !A04_AUDIO_CAPTURE

#if A04_AUDIO_START_PERSIST
    if(record)record->stage=A04_START_IRQ;
#endif
    status=OwnerConnectMaskedIrq(o);
    if (!NT_SUCCESS(status)) { o->online=FALSE;OwnerDereference(o);return status; }
#endif
#endif
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    o->diagStage=A04_STAGE_CREATED;o->diagStatus=STATUS_SUCCESS;
#endif
    *out=o;return STATUS_SUCCESS;
}
NTSTATUS Mt8167PnpBackendCreate(PDEVICE_OBJECT pdo,PRESOURCELIST resources,Mt8167PnpBackend **out)
{return CreateBackend(pdo,resources,out
#if A04_AUDIO_START_PERSIST
 ,NULL
#endif
 );}
#if A04_AUDIO_START_PERSIST
NTSTATUS Mt8167PnpBackendCreateWithRecord(PDEVICE_OBJECT pdo,PRESOURCELIST resources,Mt8167PnpBackend **out,Mt8167AudioStartRecord *record)
{return CreateBackend(pdo,resources,out,record);}
#endif

#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
static NTSTATUS CaptureAcquire(PVOID ctx,ULONG rate) {
 Mt8167PnpBackend*o=static_cast<Mt8167PnpBackend*>(ctx);int rc;
 if(!o->online)return STATUS_DEVICE_NOT_READY;
 rc=o->captureParent.AcquireInput(o->captureParent.Header.Context,rate);
 if(rc)return rc==-1?STATUS_DEVICE_BUSY:STATUS_DEVICE_HARDWARE_ERROR;
 if(o->captureParent.PrepareMaskedIrq(o->captureParent.Header.Context,o->afe.base,0x1000))
  return STATUS_DEVICE_HARDWARE_ERROR;
 return STATUS_SUCCESS;
}
static NTSTATUS CaptureRelease(PVOID ctx) {
 Mt8167PnpBackend*o=static_cast<Mt8167PnpBackend*>(ctx);
 return o->captureParent.ReleaseUnexposed(o->captureParent.Header.Context)?STATUS_DEVICE_BUSY:STATUS_SUCCESS;
}
static NTSTATUS CaptureLatency(PVOID ctx,KSRTAUDIO_HWLATENCY*p) {
 Mt8167PnpBackend*o=static_cast<Mt8167PnpBackend*>(ctx);
 return o->captureParent.QueryLatency(o->captureParent.Header.Context,&p->FifoSize,&p->ChipsetDelay,&p->CodecDelay);
}
NTSTATUS Mt8167PnpBackendCreateCapture(Mt8167PnpBackend*o,Mt8167CaptureOwner**out) {
 if(!o||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *out=NULL;OwnerLock(o);
 if(o->captureParent.Header.InterfaceDereference){OwnerUnlock(o);return STATUS_DEVICE_BUSY;}
 NTSTATUS s=o->online?Mt8167QueryCaptureParent(o->pdo,&o->captureParent):STATUS_DEVICE_NOT_READY;
 if(NT_SUCCESS(s)){
  if(o->captureParent.Header.Size!=sizeof(o->captureParent)||
     o->captureParent.Header.Version!=A04_CAPTURE_INTERFACE_VERSION||
     !o->captureParent.Header.InterfaceReference||!o->captureParent.Header.InterfaceDereference||
     !o->captureParent.AcquireInput||!o->captureParent.PrepareMaskedIrq||
     !o->captureParent.ReleaseUnexposed||!o->captureParent.QueryLatency)s=STATUS_DEVICE_CONFIGURATION_ERROR;
  else {
   Mt8167CaptureResources r={};r.pdo=o->pdo;r.afe=o->afe.base;r.adapter=o->adapter;
   r.vector=o->interrupt.vector;r.level=o->interrupt.level;r.affinity=o->interrupt.affinity;r.mode=o->interrupt.mode;
   r.context=o;r.reference=OwnerReference;r.dereference=OwnerDereference;
   r.queryLatency=CaptureLatency;r.acquireInput=CaptureAcquire;r.releaseUnexposed=CaptureRelease;
   s=Mt8167CaptureCreate(&r,out);
  }
  if(!NT_SUCCESS(s)){
   if(o->captureParent.Header.InterfaceDereference)
    o->captureParent.Header.InterfaceDereference(o->captureParent.Header.Context);
   RtlZeroMemory(&o->captureParent,sizeof(o->captureParent));
  }
 }
 OwnerUnlock(o);return s;
}
#endif
NTSTATUS Mt8167PnpBackendInterrupt(Mt8167PnpBackend *o,Mt8167PnpInterrupt *interrupt)
{
    NTSTATUS status;
    if (!o || !interrupt) return STATUS_INVALID_PARAMETER;
    OwnerLock(o);
    if (!o->online) status=STATUS_DEVICE_NOT_READY;
    else { *interrupt=o->interrupt;status=STATUS_SUCCESS; }
    OwnerUnlock(o);
    return status;
}
NTSTATUS Mt8167PnpBackendBindInterrupt(Mt8167PnpBackend *o,Mt8167Transport *t)
{
    NTSTATUS status=STATUS_DEVICE_NOT_READY;
    if (!o || !t) return STATUS_INVALID_PARAMETER;
    OwnerLock(o);
    if (o->online && t->state==AUDIO_READY && t->irq && !t->irq->interrupt) {
        t->vector=o->interrupt.vector;t->level=o->interrupt.level;
        t->affinity=o->interrupt.affinity;t->mode=o->interrupt.mode;status=STATUS_SUCCESS;
    }
    OwnerUnlock(o);return status;
}
NTSTATUS Mt8167PnpBackendAttachShared(Mt8167PnpBackend *o,const struct mt8167_audio_shared *shared)
{
    NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if (!o || !shared) return STATUS_INVALID_PARAMETER;
    OwnerLock(o);
    if (o->online && !mt8167_audio_shared_attach(&o->shared,shared)) result=STATUS_SUCCESS;
    OwnerUnlock(o);
    return result;
}
const Mt8167StreamBackend *Mt8167PnpBackendInterface(Mt8167PnpBackend *o) { return o?&o->iface:NULL; }
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
NTSTATUS Mt8167PnpBackendSnapshot(Mt8167PnpBackend *o,struct a04_audio_snapshot *s) {
    if(!o || !s || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(s,sizeof(*s));s->size=sizeof(*s);s->version=A04_AUDIO_DIAG_VERSION;
    OwnerLock(o);s->sequence=++o->diagSequence;s->stage=o->diagStage;s->last_status=(ULONG)o->diagStatus;s->failure_stage=o->diagFailedStage;
    s->online=o->online;s->transport=(ULONG)o->transport.state;
    s->irq_events=(ULONG)InterlockedCompareExchange(&o->irqEvents,0,0);
    s->irq_faults=(ULONG)InterlockedCompareExchange(&o->irqFaults,0,0);
    s->irq_active=(ULONG)InterlockedCompareExchange(&o->irq.active,0,0);
    s->shared_quarantined=o->shared.quarantined;s->digital_retained=o->digitalRetained;
    if(o->transport.dma && o->transport.dma->mdl) {
        Mt8167CommonDma *dma=o->transport.dma;
        s->dma_base=(ULONG)dma->logical.QuadPart;s->dma_bytes=dma->size;
        s->dma_end=s->dma_base+dma->size-1;s->dma_exposed=dma->exposed;s->valid|=A04_AUDIO_VALID_DMA;
    }
    if(o->online && o->parentClient.parent.ReadDiagnostic &&
       !o->parentClient.parent.ReadDiagnostic(o->parentClient.parent.Header.Context,&s->codec)) {
        s->valid|=A04_AUDIO_VALID_PARENT;
        if(s->codec.valid&A04_CODEC_VALID_AFE) {
            s->dac0=s->codec.afe_dac0;s->afe_top=s->codec.afe_top;
            s->dl1_cur=s->codec.afe_cur;s->irq_status=s->codec.afe_irq;s->valid|=A04_AUDIO_VALID_AFE;
            if(o->transport.state==AUDIO_RUNNING && (s->dac0&2u) &&
               mt8167_dl1_position(&o->transport.plan,s->dl1_cur,&s->position_bytes))s->valid|=A04_AUDIO_VALID_POSITION;
        }
    }
    OwnerUnlock(o);return STATUS_SUCCESS;
}
#endif
void Mt8167PnpBackendRevoke(Mt8167PnpBackend *o) {
    if (!o)return;
    OwnerLock(o);o->online=FALSE;
#if defined(A04_AUDIO_DMA_NOTIFICATION) && A04_AUDIO_DMA_NOTIFICATION
    Mt8167NotificationsClear(&o->notifications);o->notificationCount=0;o->notificationClient=NULL;
#endif
    OwnerUnlock(o);
    /* PnP stop/removal are serialized. GetPosition can execute at DISPATCH;
     * close admission and wait at PASSIVE before releasing any mapping. */
    ExWaitForRundownProtectionRelease(&o->positions);
    OwnerLock(o);
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
    /* Parent keeps the source masked; disconnect and flush DPC before
     * releasing its lease or invalidating the AFE mapping. */
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
    if (o->transport.state==AUDIO_RUNNING) (void)OwnerStopRunning(o);
#endif
    if (o->irq.interrupt && !o->shared.quarantined) Mt8167DisconnectIrq(&o->irq);
    if (o->shared.leased && !o->shared.quarantined) (void)mt8167_audio_shared_release_unexposed(&o->shared);
#endif
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
    /* No IRQ, DMA, register access, or asynchronous mapping user exists. */
    if (!o->shared.leased && !o->shared.quarantined) {
        if (o->afeBound && !o->parentClient.parent.BindAfe(o->parentClient.parent.Header.Context,NULL,0))
            o->afeBound=FALSE;
        if (!o->afeBound) Mt8167UnmapAssignedAfe(&o->afe);
    }
#endif
    OwnerUnlock(o);
}
void Mt8167PnpBackendRelease(Mt8167PnpBackend *o) { if(o)OwnerDereference(o); }
#endif

#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
#include "headphone_backend.inc"
#endif
