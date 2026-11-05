#include "pnp_backend.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
#include "common_dma.h"
#include "dma_adapter.h"
#include "stream_transport.h"
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
#if !defined(A04_AUDIO_QUERY_SHARED) || !A04_AUDIO_QUERY_SHARED
#error A04_AUDIO_MAP_AFE requires the shared parent query
#endif
#include "afe_mapping.h"
#endif
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
#include "audio_parent_query.h"
#endif
#define OWNER_TAG 'OwnA'
struct Mt8167PnpBackend {
    Mt8167StreamBackend iface;
    volatile LONG refs;
    FAST_MUTEX mutex;
    BOOLEAN online;
    PDEVICE_OBJECT pdo;
    PDMA_ADAPTER adapter;
    Mt8167CommonDma buffer;
    Mt8167PnpInterrupt interrupt;
    struct mt8167_audio_shared_client shared;
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
    Mt8167AfeMapping afe;
#endif
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
    struct Mt8167AudioParentClient parentClient;
#endif
};
static void OwnerReference(void *ctx) { Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx); InterlockedIncrement(&o->refs); }
static void OwnerDereference(void *ctx)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    if (InterlockedDecrement(&o->refs)) return;
    /* RUN is always refused, so all buffers allocated here remain unexposed. */
    if (o->buffer.mdl && !NT_SUCCESS(Mt8167CommonReleaseUnexposed(&o->buffer))) {
        /* Never release DMA adapter/PDO when buffer lifetime is uncertain. */
        return;
    }
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
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
    ExAcquireFastMutex(&o->mutex);
    if (!o->online) status=STATUS_DEVICE_NOT_READY;
    else if (o->buffer.mdl) status=STATUS_DEVICE_BUSY;
    else {
        status=Mt8167CommonAllocate(o->adapter,bytes,&o->buffer);
        if (NT_SUCCESS(status)) { *mdl=o->buffer.mdl;*actual=o->buffer.size;*cache=MmNonCached; }
    }
    ExReleaseFastMutex(&o->mutex);
    return status;
}
static void OwnerFree(void *ctx,PMDL mdl,ULONG bytes)
{
    Mt8167PnpBackend *o=static_cast<Mt8167PnpBackend *>(ctx);
    ExAcquireFastMutex(&o->mutex);
    if (mdl && mdl==o->buffer.mdl && bytes==o->buffer.size)
        (void)Mt8167CommonReleaseUnexposed(&o->buffer);
    ExReleaseFastMutex(&o->mutex);
}
/* The owner has no proven PWRAP/clock/IRQ ownership or DMA drain path. */
static NTSTATUS OwnerStart(void *,BOOLEAN *possiblyExposed) {
    if (!possiblyExposed) return STATUS_INVALID_PARAMETER;
    /* This diagnostic owner cannot write AFE or publish a DMA address. */
    *possiblyExposed=FALSE;return STATUS_DEVICE_NOT_READY;
}
static NTSTATUS OwnerStop(void *) { return STATUS_DEVICE_NOT_READY; }
static NTSTATUS OwnerPosition(void *,PKSAUDIO_POSITION) { return STATUS_DEVICE_NOT_READY; }
static NTSTATUS OwnerDescription(void *,PDEVICE_DESCRIPTION) { return STATUS_DEVICE_NOT_READY; }
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
    ExInitializeFastMutex(&o->mutex);
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
    o->iface.context=o;o->iface.reference=OwnerReference;o->iface.dereference=OwnerDereference;
    o->iface.allocate=OwnerAllocate;o->iface.freeBuffer=OwnerFree;
    o->iface.start=OwnerStart;o->iface.stop=OwnerStop;o->iface.position=OwnerPosition;o->iface.getDeviceDescription=OwnerDescription;
    *out=o;return STATUS_SUCCESS;
}
NTSTATUS Mt8167PnpBackendInterrupt(Mt8167PnpBackend *o,Mt8167PnpInterrupt *interrupt)
{
    NTSTATUS status;
    if (!o || !interrupt) return STATUS_INVALID_PARAMETER;
    ExAcquireFastMutex(&o->mutex);
    if (!o->online) status=STATUS_DEVICE_NOT_READY;
    else { *interrupt=o->interrupt;status=STATUS_SUCCESS; }
    ExReleaseFastMutex(&o->mutex);
    return status;
}
NTSTATUS Mt8167PnpBackendBindInterrupt(Mt8167PnpBackend *o,Mt8167Transport *t)
{
    NTSTATUS status=STATUS_DEVICE_NOT_READY;
    if (!o || !t) return STATUS_INVALID_PARAMETER;
    ExAcquireFastMutex(&o->mutex);
    if (o->online && t->state==AUDIO_READY && t->irq && !t->irq->interrupt) {
        t->vector=o->interrupt.vector;t->level=o->interrupt.level;
        t->affinity=o->interrupt.affinity;t->mode=o->interrupt.mode;status=STATUS_SUCCESS;
    }
    ExReleaseFastMutex(&o->mutex);return status;
}
NTSTATUS Mt8167PnpBackendAttachShared(Mt8167PnpBackend *o,const struct mt8167_audio_shared *shared)
{
    NTSTATUS result=STATUS_DEVICE_NOT_READY;
    if (!o || !shared) return STATUS_INVALID_PARAMETER;
    ExAcquireFastMutex(&o->mutex);
    if (o->online && !mt8167_audio_shared_attach(&o->shared,shared)) result=STATUS_SUCCESS;
    ExReleaseFastMutex(&o->mutex);
    return result;
}
const Mt8167StreamBackend *Mt8167PnpBackendInterface(Mt8167PnpBackend *o) { return o?&o->iface:NULL; }
void Mt8167PnpBackendRevoke(Mt8167PnpBackend *o) {
    if (!o)return;
    ExAcquireFastMutex(&o->mutex);o->online=FALSE;
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
    /* No IRQ, DMA, register access, or asynchronous mapping user exists. */
    Mt8167UnmapAssignedAfe(&o->afe);
#endif
    ExReleaseFastMutex(&o->mutex);
}
void Mt8167PnpBackendRelease(Mt8167PnpBackend *o) { if(o)OwnerDereference(o); }
#endif
