/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "wave_dma_probe.h"
#define WAVE_DMA_TAG 'pDaM'
struct WaveProbe {
    volatile LONG refs;
    KEVENT completed;
    PDMA_ADAPTER adapter;
    ULONG bytes;
    NTSTATUS result;
};
static void ProbeRelease(WaveProbe *p)
{ if(!InterlockedDecrement(&p->refs))ExFreePoolWithTag(p,WAVE_DMA_TAG); }
static VOID ProbeCallback(PDEVICE_OBJECT,PIRP,PSCATTER_GATHER_LIST list,PVOID context)
{
    WaveProbe *p=static_cast<WaveProbe *>(context);
    NTSTATUS result=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(list && list->NumberOfElements==1 && list->Elements[0].Length==p->bytes &&
       list->Elements[0].Address.QuadPart>0 && !(list->Elements[0].Address.QuadPart & 3) &&
       (ULONGLONG)list->Elements[0].Address.QuadPart <= 0x100000000ULL-p->bytes)
        result=STATUS_SUCCESS;
    if(list) p->adapter->DmaOperations->PutScatterGatherList(p->adapter,list,TRUE);
    p->result=result;
    KeSetEvent(&p->completed,IO_NO_INCREMENT,FALSE);
    ProbeRelease(p);
}
NTSTATUS Mt8167ProbeWaveDma(PDMA_ADAPTER adapter,PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes)
{
    WaveProbe *p;NTSTATUS result;KIRQL oldIrql;LARGE_INTEGER timeout;
    if(!adapter || !adapter->DmaOperations || !adapter->DmaOperations->GetScatterGatherList ||
       !adapter->DmaOperations->PutScatterGatherList || !pdo || !mdl || !bytes ||
       bytes>1048576 || MmGetMdlByteOffset(mdl) || MmGetMdlByteCount(mdl)<bytes ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    p=static_cast<WaveProbe *>(ExAllocatePoolWithTag(NonPagedPool,sizeof(*p),WAVE_DMA_TAG));
    if(!p)return STATUS_INSUFFICIENT_RESOURCES;
    p->refs=2;p->adapter=adapter;p->bytes=bytes;p->result=STATUS_DEVICE_CONFIGURATION_ERROR;
    KeInitializeEvent(&p->completed,NotificationEvent,FALSE);
    /* This is before the MDL is returned to the client; no concurrent CPU
     * writes are admitted during the packet-DMA mapping probe. */
    KeFlushIoBuffers(mdl,FALSE,TRUE);
    KeMemoryBarrier();
    KeRaiseIrql(DISPATCH_LEVEL,&oldIrql);
    result=adapter->DmaOperations->GetScatterGatherList(adapter,pdo,mdl,MmGetMdlVirtualAddress(mdl),
             bytes,ProbeCallback,p,TRUE);
    KeLowerIrql(oldIrql);
    if(!NT_SUCCESS(result)) { ProbeRelease(p);ProbeRelease(p);return result; }
    timeout.QuadPart=-10000000LL; /* one second, relative */
    result=KeWaitForSingleObject(&p->completed,Executive,KernelMode,FALSE,&timeout);
    if(result==STATUS_SUCCESS)result=p->result;
    else result=STATUS_IO_TIMEOUT;
    ProbeRelease(p);return result;
}
