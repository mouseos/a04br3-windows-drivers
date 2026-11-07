/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_owner_mdl.h"
#include "../dma_pages.h"
#include "../windows_hw.h"
#include <stddef.h>
#define TAG 0x644d474eu
#define CHUNK 65536u
#define MAX_BYTES 4194304u
#define MAX_SG 256u
struct a04_native_mdl_binding {
    PDMA_ADAPTER adapter;PMDL mdl;void *cpu;ULONG bytes,pages,maps,targets;
    struct a04_native_mdl_lease lease;
    PSCATTER_GATHER_LIST sg[64];uint64_t dma[1024];BOOLEAN retained,exposed;
};
struct map_wait { KEVENT event;PSCATTER_GATHER_LIST sg; };
static void mapped(PDEVICE_OBJECT dev,PIRP irp,PSCATTER_GATHER_LIST sg,PVOID context)
{
    struct map_wait *w=context;UNREFERENCED_PARAMETER(dev);UNREFERENCED_PARAMETER(irp);
    w->sg=sg;KeSetEvent(&w->event,IO_NO_INCREMENT,FALSE);
}
NTSTATUS a04_native_mdl_release_cold(struct a04_native_mdl_binding **owner)
{
    struct a04_native_mdl_binding *b;
    if(!owner || !(b=*owner) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(b->exposed || b->targets)return STATUS_DEVICE_BUSY;
    while(b->maps){b->maps--;b->adapter->DmaOperations->PutScatterGatherList(b->adapter,b->sg[b->maps],FALSE);}
    if(b->retained)b->lease.release(b->lease.context,b->mdl);
    ExFreePoolWithTag(b,TAG);*owner=NULL;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_bind(struct a04_native_mdl_binding **out,PDMA_ADAPTER a,
    PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes,const struct a04_native_mdl_lease *lease)
{
    struct a04_native_mdl_binding *b;struct a04_gpu_dma_segment *segments;
    struct map_wait w;ULONG at,i,j,pages;NTSTATUS status;PFN_NUMBER *pfns;
    if(!out || *out || !a || !pdo || !mdl || !lease || !lease->context || !lease->acquire || !lease->release ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || !bytes || bytes>MAX_BYTES || (bytes&4095u) ||
       MmGetMdlByteOffset(mdl) || MmGetMdlByteCount(mdl)!=bytes || !(mdl->MdlFlags&MDL_PAGES_LOCKED) ||
       (mdl->MdlFlags&MDL_IO_SPACE) || !MmGetMdlVirtualAddress(mdl) ||
       !a->DmaOperations || a->DmaOperations->Size<offsetof(DMA_OPERATIONS,PutScatterGatherList)+sizeof(a->DmaOperations->PutScatterGatherList) || !a->DmaOperations->GetScatterGatherList || !a->DmaOperations->PutScatterGatherList)
        return STATUS_INVALID_PARAMETER;
    pfns=MmGetMdlPfnArray(mdl);
    for(i=0;i<bytes/4096u;i++)if(!pfns[i] || (uint64_t)pfns[i]>0xfffffu)return STATUS_DEVICE_CONFIGURATION_ERROR;
    b=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*b),TAG);
    if(!b)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(b,sizeof(*b));b->adapter=a;b->mdl=mdl;b->bytes=bytes;b->pages=bytes/4096u;b->lease=*lease;
    status=lease->acquire(lease->context,mdl);if(!NT_SUCCESS(status))goto fail;
    b->retained=TRUE;b->cpu=MmGetSystemAddressForMdlSafe(mdl,NormalPagePriority);
    if(!b->cpu){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    if((uintptr_t)b->cpu&4095u){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    segments=ExAllocatePoolWithTag(NonPagedPoolNx,MAX_SG*sizeof(*segments),TAG);
    if(!segments){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    for(at=0;at<bytes;at+=CHUNK){
        ULONG n=bytes-at;if(n>CHUNK)n=CHUNK;
        KeInitializeEvent(&w.event,NotificationEvent,FALSE);w.sg=NULL;
        status=a->DmaOperations->GetScatterGatherList(a,pdo,mdl,
            (UCHAR *)MmGetMdlVirtualAddress(mdl)+at,n,mapped,&w,FALSE);
        if(!NT_SUCCESS(status))goto done;
        /* OS owns the callback until completion. Do not timeout/free a stack
         * callback context while it may still be invoked by DMA support. */
        do {status=KeWaitForSingleObject(&w.event,Executive,KernelMode,FALSE,NULL);}while(status!=STATUS_SUCCESS);
        if(!w.sg){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
        b->sg[b->maps++]=w.sg;
        if(!w.sg->NumberOfElements || w.sg->NumberOfElements>MAX_SG){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
        for(i=0;i<w.sg->NumberOfElements;i++){
            segments[i].address=(uint64_t)w.sg->Elements[i].Address.QuadPart;
            segments[i].length=w.sg->Elements[i].Length;
        }
        pages=n/4096u;
        if(a04_gpu_dma_pages(segments,w.sg->NumberOfElements,b->dma+at/4096u,pages)){
            status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
        }
        for(i=at/4096u;i<(at+n)/4096u;i++){
            if(!b->dma[i] || b->dma[i]>UINT64_C(0xfffff000) || b->dma[i]!=((uint64_t)pfns[i]<<12)){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
            for(j=0;j<i;j++)if(b->dma[i]==b->dma[j]){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
        }
    }
    status=STATUS_SUCCESS;
done:
    ExFreePoolWithTag(segments,TAG);
fail:
    if(!NT_SUCCESS(status)){struct a04_native_mdl_binding *drop=b;(void)a04_native_mdl_release_cold(&drop);return status;}
    *out=b;return STATUS_SUCCESS;
}
const uint64_t *a04_native_mdl_pages(const struct a04_native_mdl_binding *b,ULONG *count)
{ if(!b || !count)return NULL;*count=b->pages;return b->dma; }
void *a04_native_mdl_cpu(const struct a04_native_mdl_binding *b){return b?b->cpu:NULL;}
PMDL a04_native_mdl_original(const struct a04_native_mdl_binding *b){return b?b->mdl:NULL;}
NTSTATUS a04_native_mdl_sync_for_gpu(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    KeFlushIoBuffers(b->mdl,FALSE,TRUE);a04_gpu_dma_barrier();return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_sync_for_cpu(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || !b->exposed || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    KeFlushIoBuffers(b->mdl,TRUE,TRUE);a04_gpu_dma_barrier();return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_mark_exposed(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || b->exposed || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->exposed=TRUE;return STATUS_SUCCESS;
}

ULONG a04_native_mdl_extent(const struct a04_native_mdl_binding *b){return b?b->bytes:0;}
PDMA_ADAPTER a04_native_mdl_adapter(const struct a04_native_mdl_binding *b){return b?b->adapter:NULL;}
NTSTATUS a04_native_mdl_hold_cold(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || b->exposed || b->targets || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->targets=1;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_drop_cold(struct a04_native_mdl_binding *b)
{
    if(!b || b->exposed || b->targets!=1 || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->targets=0;return STATUS_SUCCESS;
}

NTSTATUS a04_native_mdl_retire_after_gpu_off(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->exposed=FALSE;return STATUS_SUCCESS;
}
