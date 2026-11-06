#include "pnp_backend.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
#include "common_dma.h"
#include "dma_adapter.h"
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
#define OWNER_TAG 'OwnA'
struct Mt8167PnpBackend {
    Mt8167StreamBackend iface;
    volatile LONG refs;
    KMUTEX mutex;
    EX_RUNDOWN_REF positions;
    EX_RUNDOWN_REF livePositions;
    BOOLEAN livePositionsClosed;
    BOOLEAN online;
    PDEVICE_OBJECT pdo;
    PDMA_ADAPTER adapter;
    Mt8167CommonDma buffer;
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
    Mt8167CommonDma silentBuffer;
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
    /* An uncertain lease release keeps the mapping and parent alive. */
    if (o->shared.leased || o->shared.quarantined || o->irq.interrupt) return;
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
    o->adapter->DmaOperations->PutDmaAdapter(o->adapter);
    ObDereferenceObject(o->pdo);
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
/* A single publish flush cannot prove live WaveRT circular-buffer coherence.
 * This platform-specific proof must be implemented before RUN can proceed. */
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
#elif defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
    if(!o || !o->buffer.mdl || !o->parentClient.parent.VerifyLivePcm ||
       !o->parentClient.parent.StartVoice || !o->parentClient.parent.StopVoice ||
       o->parentClient.parent.VerifyLivePcm(o->parentClient.parent.Header.Context,
           o->buffer.mdl,(ULONGLONG)o->buffer.logical.QuadPart,o->buffer.size))return FALSE;
    o->transport.dma=&o->buffer;return TRUE;
#else
    UNREFERENCED_PARAMETER(o);return FALSE;
#endif
}
#endif
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
static NTSTATUS OwnerStopRunning(Mt8167PnpBackend *o);
#endif
static NTSTATUS OwnerStart(void *ctx,BOOLEAN *possiblyExposed) {
    if (!ctx || !possiblyExposed) return STATUS_INVALID_PARAMETER;
    *possiblyExposed=FALSE;
    if (KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_DEVICE_NOT_READY;
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    NTSTATUS result=STATUS_DEVICE_NOT_READY;
    OwnerLock(o);
    if (o->online && o->buffer.mdl && o->shared.leased && o->irq.interrupt &&
        OwnerLiveDmaVerified(o)) {
        /* The selected period is a diagnostic half-ring, not a WaveRT
         * notification contract. No live-DMA verifier currently exists. */
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
        /* The owner mutex serializes PnP revoke. Quarantine on every
         * attempted voice operation, including a failed DMA preparation. */
        if (!o->parentClient.parent.StartVoice(o->parentClient.parent.Header.Context)) {
            if (!Mt8167TransportStartMaskedParent(&o->transport,o->buffer.size/2,&o->shared)) {
                if(o->parentClient.parent.UnmuteMin(o->parentClient.parent.Header.Context))
                    (void)OwnerStopRunning(o);
                else result=STATUS_SUCCESS;
            }
        }
        if (!o->shared.quarantined) (void)mt8167_audio_shared_quarantine(&o->shared);
#else
        if (!Mt8167TransportStartMaskedParent(&o->transport,o->buffer.size/2,&o->shared)) {
#if defined(A04_AUDIO_UNMUTE_DIAGNOSTIC) && A04_AUDIO_UNMUTE_DIAGNOSTIC
            /* Failed/ambiguous unmute still stops DMA and retains the lease. */
            if(o->parentClient.parent.UnmuteMin(o->parentClient.parent.Header.Context))
                (void)OwnerStopRunning(o);
            else
#endif
                result=STATUS_SUCCESS;
        }
#endif
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
        *possiblyExposed=(o->silentBuffer.exposed || o->shared.quarantined) ? TRUE : FALSE;
#else
        *possiblyExposed=(o->buffer.exposed || o->shared.quarantined) ? TRUE : FALSE;
#endif
    }
    A04_TRACE("A04AUD run result=%08lx online=%d buffer=%d lease=%d irq=%d live-dma=isolated-or-refused exposed=%d\n",(ULONG)result,o->online,o->buffer.mdl!=NULL,o->shared.leased,o->irq.interrupt!=NULL,*possiblyExposed);
    OwnerUnlock(o);return result;
#else
    return STATUS_DEVICE_NOT_READY;
#endif
}
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
/* Caller holds the passive-level backend mutex. Stop DL1 even if mute fails,
 * retaining resources on error. */
static NTSTATUS OwnerStopRunning(Mt8167PnpBackend *o)
{
    /* No restart follows an exposed diagnostic run. Close position admission
     * and wait for readers before disconnecting the interrupt object. */
    if(!o->livePositionsClosed) {
        ExWaitForRundownProtectionRelease(&o->livePositions);
        o->livePositionsClosed=TRUE;
    }
    int mute=o->parentClient.parent.Mute(o->parentClient.parent.Header.Context);
    int stop=Mt8167TransportStop(&o->transport);
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
    if (!mute && !stop && o->parentClient.parent.StopVoice &&
        !o->parentClient.parent.StopVoice(o->parentClient.parent.Header.Context))return STATUS_SUCCESS;
    return STATUS_DEVICE_NOT_READY;
#else
    return !mute && !stop ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
#endif
}
#endif
static NTSTATUS OwnerStop(void *ctx) {
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if (!o)return STATUS_INVALID_PARAMETER;
    OwnerLock(o);
    if (o->transport.state==AUDIO_RUNNING)result=OwnerStopRunning(o);
    A04_TRACE("A04AUD stop result=%08lx state=%d quarantine=%d irq-events=%ld irq-faults=%ld; drain=unknown\n",(ULONG)result,(int)o->transport.state,o->shared.quarantined,o->irqEvents,o->irqFaults);
    OwnerUnlock(o);return result;
#else
    UNREFERENCED_PARAMETER(ctx);return STATUS_DEVICE_NOT_READY;
#endif
}
static NTSTATUS OwnerPosition(void *ctx,PKSAUDIO_POSITION position)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    uint32_t offset;
    if (!o || !position) return STATUS_INVALID_PARAMETER;
    if (!ExAcquireRundownProtection(&o->positions)) return STATUS_DEVICE_NOT_READY;
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
static NTSTATUS OwnerDescription(void *,PDEVICE_DESCRIPTION) { return STATUS_DEVICE_NOT_READY; }
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
static void OwnerIrqNotify(void *ctx,ULONG periods,BOOLEAN fault) {
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);LONG count;
    if(!o)return;
    count=InterlockedIncrement(&o->irqEvents);
    if(fault)InterlockedIncrement(&o->irqFaults);
    if(count<=4 || fault) A04_TRACE("A04AUD irq notification=%ld periods=%lu fault=%d\n",count,periods,fault);
    UNREFERENCED_PARAMETER(periods);
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
NTSTATUS Mt8167PnpBackendCreate(PDEVICE_OBJECT pdo,PRESOURCELIST resources,Mt8167PnpBackend **out)
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
    o=static_cast<Mt8167PnpBackend *>(ExAllocatePoolWithTag(NonPagedPool,sizeof(*o),OWNER_TAG));
    if (!o) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(o,sizeof(*o));
    KeInitializeMutex(&o->mutex,0);
    ExInitializeRundownProtection(&o->positions);
    ExInitializeRundownProtection(&o->livePositions);
    status=Mt8167GetDmaAdapter(pdo,&o->adapter,&mapRegisters);
    if (!NT_SUCCESS(status)) { ExFreePoolWithTag(o,OWNER_TAG);return status; }
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
    {
        A04_SHARED_AUDIO_INTERFACE queried;
        status=Mt8167QueryAudioParent(pdo,&queried);
        if (!NT_SUCCESS(status)) { o->adapter->DmaOperations->PutDmaAdapter(o->adapter);ExFreePoolWithTag(o,OWNER_TAG);return status; }
        if (Mt8167AudioParentBind(&o->parentClient,&queried)) {
            if (queried.Header.InterfaceDereference) queried.Header.InterfaceDereference(queried.Header.Context);
            o->adapter->DmaOperations->PutDmaAdapter(o->adapter);ExFreePoolWithTag(o,OWNER_TAG);return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
        if (mt8167_audio_shared_attach(&o->shared,Mt8167AudioParentService(&o->parentClient))) {
            (void)Mt8167AudioParentClose(&o->parentClient);
            o->adapter->DmaOperations->PutDmaAdapter(o->adapter);ExFreePoolWithTag(o,OWNER_TAG);return STATUS_DEVICE_NOT_READY;
        }
    }
#endif
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
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
    o->iface.start=OwnerStart;o->iface.stop=OwnerStop;o->iface.position=OwnerPosition;o->iface.getDeviceDescription=OwnerDescription;
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
    if (o->parentClient.parent.BindAfe(o->parentClient.parent.Header.Context,o->afe.base,0x1000)) {
        o->online=FALSE;OwnerDereference(o);return STATUS_DEVICE_NOT_READY;
    }
    o->afeBound=TRUE;
    status=OwnerConnectMaskedIrq(o);
    if (!NT_SUCCESS(status)) { o->online=FALSE;OwnerDereference(o);return status; }
#endif
    *out=o;return STATUS_SUCCESS;
}
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
void Mt8167PnpBackendRevoke(Mt8167PnpBackend *o) {
    if (!o)return;
    OwnerLock(o);o->online=FALSE;OwnerUnlock(o);
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
