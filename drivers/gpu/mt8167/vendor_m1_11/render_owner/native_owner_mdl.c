/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_owner_mdl.h"
#include "../../dma_pages.h"
#include "../../windows_hw.h"
#include <stddef.h>
#define TAG 0x644d474eu
#define CHUNK 65536u
#define MAX_BYTES 4194304u
#define MAX_SG 256u
struct dma_map {
    _Alignas(8) UCHAR context[DMA_TRANSFER_CONTEXT_SIZE_V1];
    PVOID registers;PSCATTER_GATHER_LIST sg;
    ULONG register_count,offset,bytes;BOOLEAN allocated,mapped;
};
struct a04_native_mdl_binding {
    PDMA_ADAPTER adapter;PDEVICE_OBJECT pdo;PMDL mdl;void *cpu;ULONG bytes,pages,maps,targets;
    struct a04_native_mdl_lease lease;
    struct dma_map map[64];uint64_t dma[1024],physical[1024];
    BOOLEAN retained,exposed,pte_live,to_device,dma_owned,fault;
};
static NTSTATUS return_maps(struct a04_native_mdl_binding *b)
{
    PDMA_OPERATIONS ops=b->adapter->DmaOperations;
    if(b->pte_live || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    while(b->maps){
        struct dma_map *m=&b->map[b->maps-1u];NTSTATUS s;
        if(m->mapped){
            s=ops->FlushAdapterBuffersEx(b->adapter,b->mdl,m->registers,m->offset,m->bytes,b->to_device);
            if(s!=STATUS_SUCCESS){b->fault=TRUE;return s;}
            m->mapped=FALSE;
        }
        if(m->allocated){
            KIRQL old;KeRaiseIrql(DISPATCH_LEVEL,&old);
            ops->FreeMapRegisters(b->adapter,m->registers,m->register_count);
            KeLowerIrql(old);m->allocated=FALSE;
        }
        if(m->sg)ExFreePoolWithTag(m->sg,TAG);
        RtlZeroMemory(m,sizeof(*m));--b->maps;
    }
    b->dma_owned=FALSE;RtlZeroMemory(b->dma,sizeof(b->dma));return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_release_cold(struct a04_native_mdl_binding **owner)
{
    struct a04_native_mdl_binding *b;NTSTATUS s;
    if(!owner || !(b=*owner) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(b->exposed || b->pte_live || b->targets || b->fault)return STATUS_DEVICE_BUSY;
    s=return_maps(b);if(!NT_SUCCESS(s))return s;
    if(b->retained)b->lease.release(b->lease.context,b->mdl);
    ExFreePoolWithTag(b,TAG);*owner=NULL;return STATUS_SUCCESS;
}
static NTSTATUS bind_pages(struct a04_native_mdl_binding **out,PDMA_ADAPTER a,
    PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes,const struct a04_native_mdl_lease *lease,BOOLEAN to_device)
{
    struct a04_native_mdl_binding *b;ULONG i;NTSTATUS s;PFN_NUMBER *pfns;
    if(!out || *out || !a || !pdo || !mdl || !lease || !lease->context || !lease->acquire || !lease->release ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || !bytes || bytes>MAX_BYTES || (bytes&4095u) ||
       MmGetMdlByteOffset(mdl) || MmGetMdlByteCount(mdl)!=bytes || !(mdl->MdlFlags&MDL_PAGES_LOCKED) ||
       (mdl->MdlFlags&MDL_IO_SPACE) || !MmGetMdlVirtualAddress(mdl) || !a->DmaOperations ||
       a->DmaOperations->Size<offsetof(DMA_OPERATIONS,FreeAdapterObject)+sizeof(a->DmaOperations->FreeAdapterObject) ||
       !a->DmaOperations->InitializeDmaTransferContext || !a->DmaOperations->GetDmaTransferInfo ||
       !a->DmaOperations->AllocateAdapterChannelEx || !a->DmaOperations->MapTransferEx ||
       !a->DmaOperations->FlushAdapterBuffersEx || !a->DmaOperations->FreeAdapterObject || !a->DmaOperations->FreeMapRegisters)
        return STATUS_INVALID_PARAMETER;
    pfns=MmGetMdlPfnArray(mdl);
    for(i=0;i<bytes/4096u;i++)if(!pfns[i] || (uint64_t)pfns[i]>0xfffffu)return STATUS_DEVICE_CONFIGURATION_ERROR;
    b=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*b),TAG);if(!b)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(b,sizeof(*b));b->adapter=a;b->pdo=pdo;b->mdl=mdl;b->bytes=bytes;b->pages=bytes/4096u;b->lease=*lease;b->to_device=to_device;
    s=lease->acquire(lease->context,mdl);if(!NT_SUCCESS(s)){ExFreePoolWithTag(b,TAG);return s;}
    b->retained=TRUE;b->cpu=MmGetSystemAddressForMdlSafe(mdl,NormalPagePriority);
    if(!b->cpu || ((uintptr_t)b->cpu&4095u)){struct a04_native_mdl_binding *drop=b;(void)a04_native_mdl_release_cold(&drop);return STATUS_INSUFFICIENT_RESOURCES;}
    for(i=0;i<b->pages;i++)b->physical[i]=(uint64_t)pfns[i]<<12;
    /* Bind retains only CPU-owned physical backing. Never take DMA resources
     * merely to discover PFNs, and never write those PFNs into a GPU PT. */
    *out=b;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_sync_for_gpu(struct a04_native_mdl_binding *b)
{
    PDMA_OPERATIONS ops;ULONG at,i,j;NTSTATUS s=STATUS_SUCCESS;struct a04_gpu_dma_segment *segments;
    if(!b || !b->retained || b->fault || b->pte_live || b->dma_owned || b->maps || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    segments=ExAllocatePoolWithTag(NonPagedPoolNx,MAX_SG*sizeof(*segments),TAG);if(!segments)return STATUS_INSUFFICIENT_RESOURCES;
    ops=b->adapter->DmaOperations;KeFlushIoBuffers(b->mdl,FALSE,TRUE);
    for(at=0;at<b->bytes;at+=CHUNK){
        struct dma_map *m=&b->map[b->maps];DMA_TRANSFER_INFO info;ULONG length=b->bytes-at;if(length>CHUNK)length=CHUNK;
        RtlZeroMemory(&info,sizeof(info));info.Version=DMA_TRANSFER_INFO_VERSION1;
        s=ops->GetDmaTransferInfo(b->adapter,b->mdl,at,length,b->to_device,&info);if(s!=STATUS_SUCCESS)goto fail;
        if(!info.V1.MapRegisterCount || info.V1.ScatterGatherListSize<sizeof(SCATTER_GATHER_LIST) || info.V1.ScatterGatherListSize>65536u){s=STATUS_NOT_SUPPORTED;goto fail;}
        m->sg=ExAllocatePoolWithTag(NonPagedPoolNx,info.V1.ScatterGatherListSize,TAG);if(!m->sg){s=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
        ++b->maps;m->offset=at;m->bytes=length;m->register_count=info.V1.MapRegisterCount;
        s=ops->InitializeDmaTransferContext(b->adapter,m->context);if(s!=STATUS_SUCCESS)goto fail;
        /* Explicit V3 decomposition of GetSGEx: required to retain the public
         * MapRegisterBase for FlushAdapterBuffersEx. No opaque SG.Reserved. */
        s=ops->AllocateAdapterChannelEx(b->adapter,b->pdo,m->context,m->register_count,DMA_SYNCHRONOUS_CALLBACK,NULL,NULL,&m->registers);
        if(s!=STATUS_SUCCESS)goto fail;
        if(!m->registers){ops->FreeAdapterObject(b->adapter,DeallocateObject);s=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
        m->allocated=TRUE;
        s=ops->MapTransferEx(b->adapter,b->mdl,m->registers,at,0,&length,b->to_device,m->sg,info.V1.ScatterGatherListSize,NULL,NULL);
        /* The channel is no longer held while another chunk is requested.
         * Its real map registers stay owned until explicit flush/free. */
        ops->FreeAdapterObject(b->adapter,DeallocateObjectKeepRegisters);
        if(s!=STATUS_SUCCESS || length!=m->bytes){b->fault=TRUE;ExFreePoolWithTag(segments,TAG);return s==STATUS_SUCCESS?STATUS_DEVICE_CONFIGURATION_ERROR:s;}
        m->mapped=TRUE;
        if(!m->sg->NumberOfElements || m->sg->NumberOfElements>MAX_SG){s=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
        for(i=0;i<m->sg->NumberOfElements;i++){segments[i].address=(uint64_t)m->sg->Elements[i].Address.QuadPart;segments[i].length=m->sg->Elements[i].Length;}
        if(a04_gpu_dma_pages(segments,m->sg->NumberOfElements,b->dma+at/4096u,length/4096u)){s=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
        for(i=at/4096u;i<(at+length)/4096u;i++){
            if(b->dma[i]!=b->physical[i]){s=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
            for(j=0;j<i;j++)if(b->dma[i]==b->dma[j]){s=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
        }
    }
    b->dma_owned=TRUE;a04_gpu_dma_barrier();ExFreePoolWithTag(segments,TAG);return STATUS_SUCCESS;
fail:
    ExFreePoolWithTag(segments,TAG);
    {NTSTATUS clean=return_maps(b);if(!NT_SUCCESS(clean))return clean;}
    return s==STATUS_SUCCESS?STATUS_DEVICE_CONFIGURATION_ERROR:s;
}
NTSTATUS a04_native_mdl_sync_for_cpu(struct a04_native_mdl_binding *b)
{
    NTSTATUS s;
    if(!b || !b->retained || b->fault || b->pte_live || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    s=return_maps(b);if(!NT_SUCCESS(s))return s;
    KeFlushIoBuffers(b->mdl,TRUE,TRUE);a04_gpu_dma_barrier();return STATUS_SUCCESS;
}
const uint64_t *a04_native_mdl_pages(const struct a04_native_mdl_binding *b,ULONG *count)
{if(!b || !count || !b->dma_owned || b->fault)return NULL;*count=b->pages;return b->dma;}
const uint64_t *a04_native_mdl_physical_identity(const struct a04_native_mdl_binding *b,ULONG *count)
{if(!b || !count)return NULL;*count=b->pages;return b->physical;}
void *a04_native_mdl_cpu(const struct a04_native_mdl_binding *b)
{return b && !b->fault && !b->dma_owned && !b->maps && !b->pte_live?b->cpu:NULL;}
const void *a04_native_mdl_cpu_identity(const struct a04_native_mdl_binding *b){return b?b->cpu:NULL;}
PMDL a04_native_mdl_original(const struct a04_native_mdl_binding *b){return b?b->mdl:NULL;}
NTSTATUS a04_native_mdl_mark_exposed(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || !b->dma_owned || b->pte_live || b->fault || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->exposed=TRUE;b->pte_live=TRUE;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_detach_after_mmu_ack(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || !b->dma_owned || !b->pte_live || b->fault || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->pte_live=FALSE;return STATUS_SUCCESS; /* caller has zero-PTE + real FW MMUCACHE ack */
}
ULONG a04_native_mdl_extent(const struct a04_native_mdl_binding *b){return b?b->bytes:0;}
PDMA_ADAPTER a04_native_mdl_adapter(const struct a04_native_mdl_binding *b){return b?b->adapter:NULL;}
NTSTATUS a04_native_mdl_hold_cold(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || b->pte_live || b->targets || b->fault || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->targets=1;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_drop_cold(struct a04_native_mdl_binding *b)
{
    if(!b || b->exposed || b->pte_live || b->maps || b->targets!=1 || b->fault || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->targets=0;return STATUS_SUCCESS;
}
NTSTATUS a04_native_mdl_retire_after_gpu_off(struct a04_native_mdl_binding *b)
{
    if(!b || !b->retained || b->fault || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    b->pte_live=FALSE;b->exposed=FALSE;return a04_native_mdl_sync_for_cpu(b);
}
NTSTATUS a04_native_mdl_bind(struct a04_native_mdl_binding **out,PDMA_ADAPTER a,PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes,const struct a04_native_mdl_lease *lease)
{return bind_pages(out,a,pdo,mdl,bytes,lease,FALSE);}
NTSTATUS a04_native_mdl_bind_input(struct a04_native_mdl_binding **out,PDMA_ADAPTER a,PDEVICE_OBJECT pdo,PMDL mdl,ULONG bytes,const struct a04_native_mdl_lease *lease)
{return bind_pages(out,a,pdo,mdl,bytes,lease,TRUE);}

NTSTATUS a04_native_mdl_set_direction_cpu(struct a04_native_mdl_binding *b,BOOLEAN to_device)
{if(!b||!b->retained||b->fault||b->dma_owned||b->maps||b->pte_live||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;b->to_device=to_device;return STATUS_SUCCESS;}
