/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "wave_dma_live.h"
static void WaveDmaBarrier()
{
#if defined(__clang__) && defined(_ARM_)
    __asm__ volatile ("" ::: "memory");
    __builtin_arm_dmb(15);
    __asm__ volatile ("" ::: "memory");
#else
    KeMemoryBarrier();
#endif
}
#define WAVE_LIVE_TAG 'lDaM'
struct WaveRequest { volatile LONG refs,state; KEVENT completed; PDMA_ADAPTER adapter; ULONG bytes; BOOLEAN writeToDevice; NTSTATUS result; PSCATTER_GATHER_LIST list; };
static void ReleaseRequest(WaveRequest *p) { if(!InterlockedDecrement(&p->refs))ExFreePoolWithTag(p,WAVE_LIVE_TAG); }
static VOID WaveMapped(PDEVICE_OBJECT,PIRP,PSCATTER_GATHER_LIST list,PVOID context)
{
    WaveRequest *p=static_cast<WaveRequest *>(context);
    p->result=STATUS_DEVICE_CONFIGURATION_ERROR;p->list=NULL;
    if(list && list->NumberOfElements==1 && list->Elements[0].Length==p->bytes &&
       list->Elements[0].Address.QuadPart>0 && !(list->Elements[0].Address.QuadPart&3) &&
       (ULONGLONG)list->Elements[0].Address.QuadPart<=0x100000000ULL-p->bytes) {
        p->result=STATUS_SUCCESS;p->list=list;
    } else if(list) p->adapter->DmaOperations->PutScatterGatherList(p->adapter,list,p->writeToDevice);
    /* Publish the list before READY. If the caller timed out, no DMA was
     * exposed and this callback owns returning the mapping. */
    if(InterlockedCompareExchange(&p->state,1,0)!=0 && p->list)
        p->adapter->DmaOperations->PutScatterGatherList(p->adapter,p->list,p->writeToDevice);
    KeSetEvent(&p->completed,IO_NO_INCREMENT,FALSE);ReleaseRequest(p);
}
static NTSTATUS MapDirection(PDMA_ADAPTER adapter,PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes,BOOLEAN writeToDevice,Mt8167WaveDma *out)
{
    WaveRequest *p;NTSTATUS result;KIRQL old;LARGE_INTEGER timeout;
    if(!adapter || !adapter->DmaOperations || !adapter->DmaOperations->GetScatterGatherList ||
       !adapter->DmaOperations->PutScatterGatherList || !pdo || !mdl || !out || out->list ||
       !bytes || bytes>1048576 || MmGetMdlByteOffset(mdl) || MmGetMdlByteCount(mdl)<bytes ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    p=static_cast<WaveRequest *>(ExAllocatePoolWithTag(NonPagedPool,sizeof(*p),WAVE_LIVE_TAG));
    if(!p)return STATUS_INSUFFICIENT_RESOURCES;
    p->refs=2;p->state=0;p->adapter=adapter;p->bytes=bytes;p->writeToDevice=writeToDevice;p->list=NULL;p->result=STATUS_DEVICE_CONFIGURATION_ERROR;
    KeInitializeEvent(&p->completed,NotificationEvent,FALSE);KeFlushIoBuffers(mdl,!writeToDevice,TRUE);WaveDmaBarrier();
    KeRaiseIrql(DISPATCH_LEVEL,&old);
    result=adapter->DmaOperations->GetScatterGatherList(adapter,pdo,mdl,MmGetMdlVirtualAddress(mdl),bytes,WaveMapped,p,writeToDevice);
    KeLowerIrql(old);
    if(!NT_SUCCESS(result)){ReleaseRequest(p);ReleaseRequest(p);return result;}
    timeout.QuadPart=-10000000LL;
    result=KeWaitForSingleObject(&p->completed,Executive,KernelMode,FALSE,&timeout);
    if(result!=STATUS_SUCCESS && InterlockedCompareExchange(&p->state,2,0)==0) {
        ReleaseRequest(p);return STATUS_IO_TIMEOUT;
    }
    result=p->result;
    if(NT_SUCCESS(result)) {out->adapter=adapter;out->list=p->list;out->logical=p->list->Elements[0].Address;out->bytes=bytes;out->exposed=FALSE;out->writeToDevice=writeToDevice;}
    ReleaseRequest(p);return result;
}
NTSTATUS Mt8167WaveDmaMap(PDMA_ADAPTER a,PDEVICE_OBJECT p,PMDL m,ULONG n,Mt8167WaveDma *o)
{return MapDirection(a,p,m,n,TRUE,o);}
NTSTATUS Mt8167WaveDmaMapCapture(PDMA_ADAPTER a,PDEVICE_OBJECT p,PMDL m,ULONG n,Mt8167WaveDma *o)
{return MapDirection(a,p,m,n,FALSE,o);}
NTSTATUS Mt8167WaveDmaRelease(Mt8167WaveDma *m,BOOLEAN drainVerified)
{
    KIRQL old;
    if(!m || !m->adapter || !m->list || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(m->exposed && !drainVerified)return STATUS_DEVICE_BUSY;
    KeRaiseIrql(DISPATCH_LEVEL,&old);m->adapter->DmaOperations->PutScatterGatherList(m->adapter,m->list,m->writeToDevice);KeLowerIrql(old);
    RtlZeroMemory(m,sizeof(*m));return STATUS_SUCCESS;
}

NTSTATUS Mt8167WaveDmaReleaseStopped(Mt8167WaveDma *m)
{
    KIRQL old;
    if(!m || !m->adapter || !m->list || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    /* Caller has this exact lease's source-contract stop receipt. There is
     * no timeout or inferred physical fabric-idle bit at this boundary. */
    KeRaiseIrql(DISPATCH_LEVEL,&old);
    m->adapter->DmaOperations->PutScatterGatherList(m->adapter,m->list,m->writeToDevice);
    KeLowerIrql(old);RtlZeroMemory(m,sizeof(*m));return STATUS_SUCCESS;
}
