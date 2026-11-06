/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 GCE PDO resource owner. No clock, IRQ, event or command submission
 * is enabled here. A display-adapter DMA address is never accepted. */
#include <ntddk.h>
#include <wdf.h>
#include "../wddm-display/ge8300_display_resource_check.h"
#ifndef A04_GCE_HANDOFF_OBSERVE
#define A04_GCE_HANDOFF_OBSERVE 0
#endif
#ifndef A04_GCE_PARENT_CLOCK_INTERFACE
#define A04_GCE_PARENT_CLOCK_INTERFACE 0
#endif
#if A04_GCE_PARENT_CLOCK_INTERFACE
#include "gce_clock_interface.h"
#endif
#ifndef A04_GCE_DISPLAY_LEASE_INTERFACE
#define A04_GCE_DISPLAY_LEASE_INTERFACE 0
#endif
#ifndef A04_GCE_IRQ_ATTACH
#define A04_GCE_IRQ_ATTACH 0
#endif
#if A04_GCE_IRQ_ATTACH && (!A04_GCE_PARENT_CLOCK_INTERFACE || !A04_GCE_DISPLAY_LEASE_INTERFACE)
#error GCE IRQ attach requires parent clock and display lease interfaces
#endif
#if A04_GCE_DISPLAY_LEASE_INTERFACE
#include "gce_display_interface.h"
#include "gce_display_lease.h"
#endif
#if A04_GCE_IRQ_ATTACH
#include "gce_submit.h"
#include "gce_observe.h"
#endif
#define GCE_BYTES 4096u
/* These proofs require independent providers; assigning PnP resources does
 * not establish firmware release or the lifetime of the scanout surface. */
#define GCE_PROOF_POWER_CLOCK 0x01u
#define GCE_PROOF_CONTROLLER  0x02u
#define GCE_PROOF_IRQ_THREAD_EVENT 0x04u
#define GCE_PROOF_DISPLAY 0x08u
#define GCE_PROOF_DMA_LIFETIME 0x10u
#define GCE_PROOF_ALL (GCE_PROOF_POWER_CLOCK|GCE_PROOF_CONTROLLER|GCE_PROOF_IRQ_THREAD_EVENT|GCE_PROOF_DISPLAY|GCE_PROOF_DMA_LIFETIME)
#define GCE_PROOF_IRQ (GCE_PROOF_POWER_CLOCK|GCE_PROOF_CONTROLLER|GCE_PROOF_IRQ_THREAD_EVENT)
#if A04_GCE_HANDOFF_OBSERVE
typedef struct { ULONG version,bytes,valid,top2_status,thread0_enable,thread0_status,thread0_irq,ownership; } GCE_OBSERVATION;
static VOID GceObserveFirmware(VOID)
{
    UNICODE_STRING name=RTL_CONSTANT_STRING(L"A04BR3GceObservation");
    GUID guid={0xb0f4cc26,0x0ad6,0x48e9,{0x94,0x74,0xe0,0x37,0x2a,0xcf,0x32,0x41}};
    GCE_OBSERVATION record;ULONG size=sizeof(record),attributes=0;NTSTATUS status;
    RtlZeroMemory(&record,sizeof(record));
    status=ExGetFirmwareEnvironmentVariable(&name,&guid,&record,&size,&attributes);
    if(!NT_SUCCESS(status) || size!=sizeof(record) || record.version!=1 || record.bytes!=sizeof(record) ||
       record.valid!=1 || record.ownership || record.thread0_enable!=MAXULONG ||
       record.thread0_status!=MAXULONG || record.thread0_irq!=MAXULONG) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,"A04GCE handoff unavailable/invalid status=%08lx size=%lu\n",(ULONG)status,size);
        return;
    }
    /* Diagnostic only: a TOP2 snapshot cannot authorize any GCE access. */
    DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,"A04GCE handoff TOP2=%08lx thread/irq/secure ownership unknown\n",record.top2_status);
}
#endif
typedef struct gce_context {
    PDMA_ADAPTER adapter;
    PDEVICE_OBJECT pdo;
    volatile ULONG *registers;
    PVOID command_cpu;
    PHYSICAL_ADDRESS command_dma;
    ULONG map_registers;
#if A04_GCE_IRQ_ATTACH
    WDFINTERRUPT interrupt;
    struct a04_gce_submit engine;
    KEVENT command_done;
    volatile LONG irq_enabled;
    volatile LONG removal_pending;
    /* No current provider can set these proofs; PnP resources are not proof. */
    ULONG ownership_proofs;
#endif
#if A04_GCE_DISPLAY_LEASE_INTERFACE
    WDFWAITLOCK lease_lock;
    struct a04_gce_display_lease lease;
#endif
#if A04_GCE_PARENT_CLOCK_INTERFACE
    WDFIOTARGET parent_target;
    A04_GCE_CLOCK_INTERFACE parent_clock;
    ULONG clock_token;NTSTATUS clock_status;
#endif
} GCE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(GCE_CONTEXT,GceContext)
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD GceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE GcePrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE GceRelease;
#if A04_GCE_DISPLAY_LEASE_INTERFACE
EVT_WDF_DEVICE_SURPRISE_REMOVAL GceSurprise;
EVT_WDF_DEVICE_D0_ENTRY GceD0Entry;
EVT_WDF_DEVICE_D0_EXIT GceD0Exit;
#if A04_GCE_IRQ_ATTACH
EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED GceD0PostInterrupt;
static NTSTATUS GceDisplaySubmit(PVOID,ULONG,ULONG,ULONG,ULONG);
static NTSTATUS GceDisplayObserve(PVOID,ULONG,A04_GCE_THREAD_OBSERVATION *);
static NTSTATUS GceDisplayRestore(PVOID,ULONG,ULONG,ULONG,ULONG,int (*)(void *,uint32_t,uint32_t),void *);
#else
static NTSTATUS GceDisplayRestore(PVOID p,ULONG t,ULONG e,ULONG a,ULONG b,int (*v)(void *,uint32_t,uint32_t),void *x)
{UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(t);UNREFERENCED_PARAMETER(e);UNREFERENCED_PARAMETER(a);UNREFERENCED_PARAMETER(b);UNREFERENCED_PARAMETER(v);UNREFERENCED_PARAMETER(x);return STATUS_NOT_SUPPORTED;}
static NTSTATUS GceDisplaySubmit(PVOID p,ULONG token,ULONG event,ULONG l0,ULONG l3)
{UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(token);UNREFERENCED_PARAMETER(event);UNREFERENCED_PARAMETER(l0);UNREFERENCED_PARAMETER(l3);return STATUS_NOT_SUPPORTED;}
static NTSTATUS GceDisplayObserve(PVOID p,ULONG token,A04_GCE_THREAD_OBSERVATION *out)
{UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(token);UNREFERENCED_PARAMETER(out);return STATUS_NOT_SUPPORTED;}
#endif
static VOID GceInterfaceReference(PVOID p){WdfObjectReference((WDFDEVICE)p);}
static VOID GceInterfaceDereference(PVOID p){WdfObjectDereference((WDFDEVICE)p);}
static NTSTATUS GceDisplayAcquire(PVOID p,ULONG *token)
{
    GCE_CONTEXT *c;NTSTATUS status=STATUS_DEVICE_NOT_READY;
    if(!p || !token || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    *token=0;c=GceContext((WDFDEVICE)p);(void)WdfWaitLockAcquire(c->lease_lock,NULL);
    if(a04_gce_display_acquire(&c->lease,(uint32_t *)token)) {
        WdfObjectReference((WDFDEVICE)p);status=STATUS_SUCCESS;
    }
    WdfWaitLockRelease(c->lease_lock);return status;
}
static NTSTATUS GceDisplayRelease(PVOID p,ULONG token)
{
    GCE_CONTEXT *c;BOOLEAN drop=FALSE;
    if(!p || !token || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    c=GceContext((WDFDEVICE)p);(void)WdfWaitLockAcquire(c->lease_lock,NULL);
#if A04_GCE_IRQ_ATTACH
    if(c->engine.state!=A04_GCE_IDLE){WdfWaitLockRelease(c->lease_lock);return STATUS_DEVICE_BUSY;}
#endif
    if(a04_gce_display_release(&c->lease,token))drop=TRUE;
    WdfWaitLockRelease(c->lease_lock);
    if(drop){WdfObjectDereference((WDFDEVICE)p);return STATUS_SUCCESS;}
    return STATUS_INVALID_DEVICE_STATE;
}
static NTSTATUS GceDisplayReady(PVOID p,ULONG token)
{
    GCE_CONTEXT *c;NTSTATUS status=STATUS_DEVICE_NOT_READY;
    if(!p || !token || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    c=GceContext((WDFDEVICE)p);(void)WdfWaitLockAcquire(c->lease_lock,NULL);
    if(!a04_gce_display_is_live(&c->lease,token))status=STATUS_INVALID_DEVICE_STATE;
#if A04_GCE_IRQ_ATTACH
    else if(c->ownership_proofs==GCE_PROOF_ALL && c->clock_token && NT_SUCCESS(c->clock_status) &&
            InterlockedCompareExchange(&c->irq_enabled,0,0) && c->interrupt && c->engine.state==A04_GCE_IDLE)
        status=STATUS_SUCCESS;
#endif
    /* A resource lease never proves clock, IRQ, thread or event ownership. */
    WdfWaitLockRelease(c->lease_lock);return status;
}
VOID GceSurprise(WDFDEVICE device)
{
    GCE_CONTEXT *c=GceContext(device);
#if A04_GCE_IRQ_ATTACH
    InterlockedExchange(&c->removal_pending,1);
#endif
    (void)WdfWaitLockAcquire(c->lease_lock,NULL);
    c->lease.removed=1;c->lease.online=0;WdfWaitLockRelease(c->lease_lock);
}
NTSTATUS GceD0Entry(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
    GCE_CONTEXT *c=GceContext(device);UNREFERENCED_PARAMETER(previous);
    (void)WdfWaitLockAcquire(c->lease_lock,NULL);
#if !A04_GCE_IRQ_ATTACH
    if(!c->lease.removed && c->adapter && c->command_cpu && c->registers)c->lease.online=1;
#else
    /* EvtInterruptEnable currently refuses unproved ownership. */
    c->lease.online=0;
#endif
    WdfWaitLockRelease(c->lease_lock);return STATUS_SUCCESS;
}
NTSTATUS GceD0Exit(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
    GCE_CONTEXT *c=GceContext(device);UNREFERENCED_PARAMETER(target);
    (void)WdfWaitLockAcquire(c->lease_lock,NULL);c->lease.online=0;WdfWaitLockRelease(c->lease_lock);
#if A04_GCE_IRQ_ATTACH
    if(c->engine.state!=A04_GCE_IDLE)return STATUS_DEVICE_BUSY;
#endif
    return STATUS_SUCCESS;
}
#if A04_GCE_IRQ_ATTACH
NTSTATUS GceD0PostInterrupt(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
    GCE_CONTEXT *c=GceContext(device);UNREFERENCED_PARAMETER(previous);
    (void)WdfWaitLockAcquire(c->lease_lock,NULL);
    if(!c->lease.removed && c->adapter && c->command_cpu && c->registers &&
       InterlockedCompareExchange(&c->irq_enabled,0,0))c->lease.online=1;
    WdfWaitLockRelease(c->lease_lock);return STATUS_SUCCESS;
}
#endif
#endif
#if A04_GCE_IRQ_ATTACH
EVT_WDF_INTERRUPT_ISR GceInterruptIsr;
EVT_WDF_INTERRUPT_ENABLE GceInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE GceInterruptDisable;
EVT_WDF_INTERRUPT_DPC GceInterruptDpc;
EVT_WDF_INTERRUPT_SYNCHRONIZE GceKickSync;
EVT_WDF_INTERRUPT_SYNCHRONIZE GceTimeoutSync;
EVT_WDF_INTERRUPT_SYNCHRONIZE GceStateSync;
EVT_WDF_INTERRUPT_SYNCHRONIZE GceObserveSync;
static int GceCanInspect(void *p)
{
    GCE_CONTEXT *c=(GCE_CONTEXT *)p;
    return c && !InterlockedCompareExchange(&c->removal_pending,0,0) &&
        (c->ownership_proofs&GCE_PROOF_IRQ)==GCE_PROOF_IRQ && c->clock_token && NT_SUCCESS(c->clock_status) &&
        InterlockedCompareExchange(&c->irq_enabled,0,0) && c->registers && c->command_cpu && c->adapter;
}
static int GceAuthorized(void *p)
{
    GCE_CONTEXT *c=(GCE_CONTEXT *)p;
    return GceCanInspect(p) && c->ownership_proofs==GCE_PROOF_ALL;
}
static int GceRead(void *p,uint32_t off,uint32_t *v)
{
    GCE_CONTEXT *c=(GCE_CONTEXT *)p;if(!c || InterlockedCompareExchange(&c->removal_pending,0,0) || !c->registers || !v || off>0xffcu || (off&3u))return -1;
    *v=READ_REGISTER_ULONG(c->registers+off/4);return 0;
}
static int GceWrite(void *p,uint32_t off,uint32_t v)
{
    GCE_CONTEXT *c=(GCE_CONTEXT *)p;if(!c || InterlockedCompareExchange(&c->removal_pending,0,0) || !c->registers || off>0xffcu || (off&3u))return -1;
    WRITE_REGISTER_ULONG(c->registers+off/4,v);return 0;
}
static void GceBarrier(void *p)
{
    UNREFERENCED_PARAMETER(p);
#if defined(__clang__) && defined(_ARM_)
    __asm__ volatile ("" ::: "memory");
    __builtin_arm_dmb(15);
    __asm__ volatile ("" ::: "memory");
#else
    KeMemoryBarrier();
#endif
}
static void GceDelay(void *p){UNREFERENCED_PARAMETER(p);KeStallExecutionProcessor(1);}
BOOLEAN GceKickSync(WDFINTERRUPT irq,WDFCONTEXT p)
{UNREFERENCED_PARAMETER(irq);return a04_gce_kick_primary(&((GCE_CONTEXT *)p)->engine)==0;}
BOOLEAN GceTimeoutSync(WDFINTERRUPT irq,WDFCONTEXT p)
{UNREFERENCED_PARAMETER(irq);a04_gce_submit_timeout(&((GCE_CONTEXT *)p)->engine);return TRUE;}
typedef struct {GCE_CONTEXT *owner;uint32_t state;} GCE_STATE_QUERY;
BOOLEAN GceStateSync(WDFINTERRUPT irq,WDFCONTEXT p)
{GCE_STATE_QUERY *q=(GCE_STATE_QUERY *)p;UNREFERENCED_PARAMETER(irq);q->state=q->owner->engine.state;return TRUE;}
typedef struct {GCE_CONTEXT *owner;A04_GCE_THREAD_OBSERVATION value;} GCE_OBSERVE_QUERY;
BOOLEAN GceObserveSync(WDFINTERRUPT irq,WDFCONTEXT p)
{
    GCE_OBSERVE_QUERY *q=(GCE_OBSERVE_QUERY *)p;GCE_CONTEXT *c=q->owner;struct a04_gce_snapshot s;
    UNREFERENCED_PARAMETER(irq);
    if(a04_gce_observe_thread0(c,GceCanInspect(c) && c->engine.state==A04_GCE_IDLE,GceRead,&s))return FALSE;
    q->value.enable=s.enable;q->value.status=s.status;q->value.irq_status=s.irq_status;q->value.irq_enable=s.irq_enable;return TRUE;
}
BOOLEAN GceInterruptIsr(WDFINTERRUPT interrupt,ULONG message)
{
    GCE_CONTEXT *c=GceContext(WdfInterruptGetDevice(interrupt));int result;
    UNREFERENCED_PARAMETER(message);
    if(!c || InterlockedCompareExchange(&c->removal_pending,0,0) || c->engine.state!=A04_GCE_RUNNING || !InterlockedCompareExchange(&c->irq_enabled,0,0))return FALSE;
    result=a04_gce_submit_irq(&c->engine);
    if(result==1)return FALSE;
    WdfInterruptQueueDpcForIsr(interrupt);
    /* -1 means an uncertain read/ack: retain and do not claim the IRQ. */
    return (result==0 || result==-2)?TRUE:FALSE;
}
VOID GceInterruptDpc(WDFINTERRUPT interrupt,WDFOBJECT object)
{GCE_CONTEXT *c=GceContext(WdfInterruptGetDevice(interrupt));UNREFERENCED_PARAMETER(object);KeSetEvent(&c->command_done,IO_NO_INCREMENT,FALSE);}
NTSTATUS GceInterruptEnable(WDFINTERRUPT interrupt,WDFDEVICE device)
{
    GCE_CONTEXT *c=GceContext(device);UNREFERENCED_PARAMETER(interrupt);
    /* No current source sets ownership_proofs. No MMIO on refusal. */
    if(!c || (c->ownership_proofs&GCE_PROOF_IRQ)!=GCE_PROOF_IRQ || !c->clock_token || !NT_SUCCESS(c->clock_status))return STATUS_DEVICE_NOT_READY;
    InterlockedExchange(&c->irq_enabled,1);return STATUS_SUCCESS;
}
NTSTATUS GceInterruptDisable(WDFINTERRUPT interrupt,WDFDEVICE device)
{
    GCE_CONTEXT *c=GceContext(device);UNREFERENCED_PARAMETER(interrupt);
    InterlockedExchange(&c->irq_enabled,0);
    if(c->engine.state!=A04_GCE_IDLE){a04_gce_submit_timeout(&c->engine);return STATUS_DEVICE_BUSY;}
    return STATUS_SUCCESS;
}
static NTSTATUS GceDisplayRun(PVOID p,ULONG token,ULONG event,ULONG l0,ULONG l3,
                               int (*validate)(void *,uint32_t,uint32_t),void *validation_context,BOOLEAN restore)
{
    GCE_CONTEXT *c;NTSTATUS status;LARGE_INTEGER timeout;GCE_STATE_QUERY q;
    if(!p || !token || KeGetCurrentIrql()!=PASSIVE_LEVEL || (restore && !validate))return STATUS_INVALID_PARAMETER;
    c=GceContext((WDFDEVICE)p);(void)WdfWaitLockAcquire(c->lease_lock,NULL);
    if(!a04_gce_display_is_live(&c->lease,token) || !c->interrupt || !GceAuthorized(c) ||
       c->engine.state!=(restore?A04_GCE_STOPPED:A04_GCE_IDLE)) {
        status=STATUS_DEVICE_NOT_READY;goto done;
    }
    KeClearEvent(&c->command_done);
    if(restore ? a04_gce_prepare_restore(&c->engine,event,l0,l3,validate,validation_context) :
                 a04_gce_prepare_primary(&c->engine,event,l0,l3)) {status=STATUS_IO_DEVICE_ERROR;goto done;}
    if(!WdfInterruptSynchronize(c->interrupt,GceKickSync,c)) {status=STATUS_IO_DEVICE_ERROR;goto done;}
    timeout.QuadPart=-20000000LL;
    status=KeWaitForSingleObject(&c->command_done,Executive,KernelMode,FALSE,&timeout);
    if(status!=STATUS_SUCCESS) {
        (void)WdfInterruptSynchronize(c->interrupt,GceTimeoutSync,c);
        status=status==STATUS_TIMEOUT?STATUS_IO_TIMEOUT:status;goto done;
    }
    q.owner=c;q.state=A04_GCE_RETAIN;
    if(!WdfInterruptSynchronize(c->interrupt,GceStateSync,&q) || q.state!=A04_GCE_IRQ_DONE)
        status=STATUS_IO_DEVICE_ERROR;
    else if(a04_gce_stop_completed(&c->engine))status=STATUS_IO_DEVICE_ERROR;
    /* EOC and stop prove neither latch nor surface retirement. */
done:
    WdfWaitLockRelease(c->lease_lock);return status;
}
static NTSTATUS GceDisplaySubmit(PVOID p,ULONG token,ULONG event,ULONG l0,ULONG l3)
{return GceDisplayRun(p,token,event,l0,l3,NULL,NULL,FALSE);}
static NTSTATUS GceDisplayRestore(PVOID p,ULONG token,ULONG event,ULONG l0,ULONG l3,
                                  int (*validate)(void *,uint32_t,uint32_t),void *validation_context)
{return GceDisplayRun(p,token,event,l0,l3,validate,validation_context,TRUE);}
static NTSTATUS GceDisplayObserve(PVOID p,ULONG token,A04_GCE_THREAD_OBSERVATION *out)
{
    GCE_CONTEXT *c;GCE_OBSERVE_QUERY q;NTSTATUS status=STATUS_DEVICE_NOT_READY;
    if(!p || !token || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));c=GceContext((WDFDEVICE)p);
    (void)WdfWaitLockAcquire(c->lease_lock,NULL);
    if(a04_gce_display_is_live(&c->lease,token) && c->interrupt && GceCanInspect(c)) {
        RtlZeroMemory(&q,sizeof(q));q.owner=c;
        if(WdfInterruptSynchronize(c->interrupt,GceObserveSync,&q)) {*out=q.value;status=STATUS_SUCCESS;}
    }
    WdfWaitLockRelease(c->lease_lock);return status;
}
#endif
#if A04_GCE_PARENT_CLOCK_INTERFACE
static NTSTATUS GceConnectParent(WDFDEVICE device,struct gce_context *c)
{
    PWSTR links=NULL;UNICODE_STRING path;WDF_IO_TARGET_OPEN_PARAMS open;
    WDF_OBJECT_ATTRIBUTES attributes;NTSTATUS status;size_t length;
    status=IoGetDeviceInterfaces((LPGUID)&A04GceParentDeviceInterfaceGuid,NULL,0,&links);
    if(!NT_SUCCESS(status))return status;
    if(!links || !*links){status=STATUS_NO_SUCH_DEVICE;goto done;}
    RtlInitUnicodeString(&path,links);length=path.Length/sizeof(WCHAR);
    /* No arbitrary selection if multiple parents appear. */
    if(links[length+1]){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;
    status=WdfIoTargetCreate(device,&attributes,&c->parent_target);
    if(!NT_SUCCESS(status))goto done;
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,GENERIC_READ|GENERIC_WRITE);
    status=WdfIoTargetOpen(c->parent_target,&open);
    if(!NT_SUCCESS(status))goto fail;
    RtlZeroMemory(&c->parent_clock,sizeof(c->parent_clock));
    status=WdfIoTargetQueryForInterface(c->parent_target,&A04GceClockInterfaceGuid,
        (PINTERFACE)&c->parent_clock,sizeof(c->parent_clock),A04_GCE_CLOCK_INTERFACE_VERSION,NULL);
    if(!NT_SUCCESS(status))goto close;
    if(!c->parent_clock.Header.InterfaceDereference || !c->parent_clock.Acquire || !c->parent_clock.Release) {
        status=STATUS_NOINTERFACE;goto dereference;
    }
    /* The parent currently reports not-ready because firmware ownership of
     * the shared clock has not been proved; no GCE access is enabled. */
    status=c->parent_clock.Acquire(c->parent_clock.Header.Context,&c->clock_token);
    if(c->clock_token)goto done; /* retain even when acquisition returned error */
    if(NT_SUCCESS(status))status=STATUS_DEVICE_CONFIGURATION_ERROR;
dereference:
    if(c->parent_clock.Header.InterfaceDereference)c->parent_clock.Header.InterfaceDereference(c->parent_clock.Header.Context);
    RtlZeroMemory(&c->parent_clock,sizeof(c->parent_clock));
close:
    WdfIoTargetClose(c->parent_target);
fail:
    WdfObjectDelete(c->parent_target);c->parent_target=NULL;
done:
    if(links)ExFreePool(links);return status;
}
#endif
static void GceFree(struct gce_context *c)
{
    if(c->command_cpu && c->adapter && c->adapter->DmaOperations &&
       c->adapter->DmaOperations->FreeCommonBuffer) {
        c->adapter->DmaOperations->FreeCommonBuffer(c->adapter,GCE_BYTES,c->command_dma,c->command_cpu,FALSE);
        c->command_cpu=NULL;
    }
    if(c->adapter && c->adapter->DmaOperations &&
       c->adapter->DmaOperations->Size>=FIELD_OFFSET(DMA_OPERATIONS,PutDmaAdapter)+sizeof(c->adapter->DmaOperations->PutDmaAdapter) &&
       c->adapter->DmaOperations->PutDmaAdapter) {
        c->adapter->DmaOperations->PutDmaAdapter(c->adapter);c->adapter=NULL;
    }
    if(c->registers) {MmUnmapIoSpace((PVOID)c->registers,0x1000);c->registers=NULL;}
    c->pdo=NULL;c->map_registers=0;c->command_dma.QuadPart=0;
}
NTSTATUS GcePrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
    struct gce_context *c=GceContext(device);PCM_PARTIAL_RESOURCE_DESCRIPTOR mem=NULL,irq=NULL;
    DEVICE_DESCRIPTION desc;ULONG n,i,matches=0,interrupts=0;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    UNREFERENCED_PARAMETER(raw);
#if A04_GCE_HANDOFF_OBSERVE
    GceObserveFirmware();
#endif
    if(c->adapter || c->registers || c->command_cpu)return STATUS_INVALID_DEVICE_STATE;
    n=WdfCmResourceListGetCount(translated);
    for(i=0;i<n;i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        if(!d)continue;
        if(d->Type==CmResourceTypeMemory && (ULONGLONG)d->u.Memory.Start.QuadPart==0x1020a000ULL){mem=d;matches++;}
        if(d->Type==CmResourceTypeInterrupt){irq=d;interrupts++;}
    }
    if(!mem || !irq || !a04_gce_resources_valid(matches,interrupts,
        mem->ShareDisposition==CmResourceShareDeviceExclusive,
        irq->ShareDisposition==CmResourceShareDeviceExclusive,
        (uint64_t)mem->u.Memory.Start.QuadPart,mem->u.Memory.Length))return status;
    c->pdo=WdfDeviceWdmGetPhysicalDevice(device);
    if(!c->pdo)return STATUS_INVALID_DEVICE_STATE;
    RtlZeroMemory(&desc,sizeof(desc));desc.Version=DEVICE_DESCRIPTION_VERSION3;
    desc.Master=TRUE;desc.ScatterGather=FALSE;desc.DmaAddressWidth=32;
    desc.InterfaceType=Internal;desc.MaximumLength=GCE_BYTES;
    c->adapter=IoGetDmaAdapter(c->pdo,&desc,&c->map_registers);
    if(!c->adapter || !c->adapter->DmaOperations ||
       c->adapter->DmaOperations->Size<FIELD_OFFSET(DMA_OPERATIONS,FreeCommonBuffer)+sizeof(c->adapter->DmaOperations->FreeCommonBuffer) ||
       !c->adapter->DmaOperations->AllocateCommonBuffer || !c->adapter->DmaOperations->FreeCommonBuffer ||
       !c->adapter->DmaOperations->PutDmaAdapter || !c->map_registers) {status=STATUS_NOT_SUPPORTED;goto fail;}
    c->command_cpu=c->adapter->DmaOperations->AllocateCommonBuffer(c->adapter,GCE_BYTES,&c->command_dma,FALSE);
    if(!c->command_cpu){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    if(!c->command_dma.QuadPart || c->command_dma.QuadPart>0xfffff000LL ||
       (c->command_dma.LowPart&4095u) || ((ULONG_PTR)c->command_cpu&4095u)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    c->registers=(volatile ULONG *)MmMapIoSpace(mem->u.Memory.Start,0x1000,MmNonCached);
    if(!c->registers){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
#if A04_GCE_IRQ_ATTACH
    c->engine.context=c;c->engine.authorize=GceAuthorized;
    c->engine.read32=GceRead;c->engine.write32=GceWrite;
    c->engine.barrier=GceBarrier;c->engine.delay=GceDelay;
    c->engine.command_cpu=(uint32_t *)c->command_cpu;
    c->engine.command_dma=(uint64_t)c->command_dma.QuadPart;
    c->engine.state=A04_GCE_IDLE;
#endif
    /* No device DMA, MMIO access or interrupt is initiated. */
#if A04_GCE_PARENT_CLOCK_INTERFACE
    c->clock_status=GceConnectParent(device,c);
#endif
    return STATUS_SUCCESS;
fail:
    GceFree(c);return status;
}
NTSTATUS GceRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
    struct gce_context *c=GceContext(device);UNREFERENCED_PARAMETER(translated);
#if A04_GCE_DISPLAY_LEASE_INTERFACE
    (void)WdfWaitLockAcquire(c->lease_lock,NULL);c->lease.online=0;
#if A04_GCE_IRQ_ATTACH
    if(c->engine.state!=A04_GCE_IDLE){WdfWaitLockRelease(c->lease_lock);return STATUS_DEVICE_BUSY;}
#endif
    if(c->lease.active){WdfWaitLockRelease(c->lease_lock);return STATUS_DEVICE_BUSY;}
    WdfWaitLockRelease(c->lease_lock);
#endif
#if A04_GCE_PARENT_CLOCK_INTERFACE
    if(c->clock_token)return STATUS_DEVICE_BUSY;
#endif
    GceFree(c);return STATUS_SUCCESS;
}
NTSTATUS GceAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS callbacks;WDF_OBJECT_ATTRIBUTES attributes;WDFDEVICE device;
#if A04_GCE_DISPLAY_LEASE_INTERFACE
    A04_GCE_DISPLAY_INTERFACE iface;WDF_QUERY_INTERFACE_CONFIG query;NTSTATUS status;
#endif
#if A04_GCE_IRQ_ATTACH
    WDF_INTERRUPT_CONFIG irqConfig;WDFINTERRUPT irq;
#endif
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);callbacks.EvtDevicePrepareHardware=GcePrepare;
    callbacks.EvtDeviceReleaseHardware=GceRelease;
#if A04_GCE_DISPLAY_LEASE_INTERFACE
    callbacks.EvtDeviceSurpriseRemoval=GceSurprise;
    callbacks.EvtDeviceD0Entry=GceD0Entry;callbacks.EvtDeviceD0Exit=GceD0Exit;
#if A04_GCE_IRQ_ATTACH
    callbacks.EvtDeviceD0EntryPostInterruptsEnabled=GceD0PostInterrupt;
#endif
#endif
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&callbacks);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,GCE_CONTEXT);
#if A04_GCE_DISPLAY_LEASE_INTERFACE
    status=WdfDeviceCreate(&init,&attributes,&device);if(!NT_SUCCESS(status))return status;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;
    status=WdfWaitLockCreate(&attributes,&GceContext(device)->lease_lock);
    if(!NT_SUCCESS(status))return status;
#if A04_GCE_IRQ_ATTACH
    KeInitializeEvent(&GceContext(device)->command_done,NotificationEvent,FALSE);
    WDF_INTERRUPT_CONFIG_INIT(&irqConfig,GceInterruptIsr,GceInterruptDpc);
    irqConfig.EvtInterruptEnable=GceInterruptEnable;irqConfig.EvtInterruptDisable=GceInterruptDisable;
    irqConfig.AutomaticSerialization=FALSE;
    status=WdfInterruptCreate(device,&irqConfig,WDF_NO_OBJECT_ATTRIBUTES,&irq);
    if(!NT_SUCCESS(status))return status;
    GceContext(device)->interrupt=irq;
#endif
    RtlZeroMemory(&iface,sizeof(iface));iface.Header.Size=sizeof(iface);
    iface.Header.Version=A04_GCE_DISPLAY_INTERFACE_VERSION;iface.Header.Context=device;
    iface.Header.InterfaceReference=GceInterfaceReference;iface.Header.InterfaceDereference=GceInterfaceDereference;
    iface.Acquire=GceDisplayAcquire;iface.Release=GceDisplayRelease;iface.Ready=GceDisplayReady;iface.Submit=GceDisplaySubmit;iface.Restore=GceDisplayRestore;iface.ObserveThread0=GceDisplayObserve;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&A04GceDisplayInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(device,&query);if(!NT_SUCCESS(status))return status;
    return WdfDeviceCreateDeviceInterface(device,&A04GceDeviceInterfaceGuid,NULL);
#else
    return WdfDeviceCreate(&init,&attributes,&device);
#endif
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,GceAdd);
    return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
