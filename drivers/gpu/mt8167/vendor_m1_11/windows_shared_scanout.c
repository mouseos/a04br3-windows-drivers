/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_shared_scanout.h"
#include "../dma_pages.h"
#include "../gpu_mmu.h"
#define TAG 0x53444741u
#if !defined(A04_SHARED_SCANOUT_VIDMM_ONLY) || !A04_SHARED_SCANOUT_VIDMM_ONLY
struct map_wait { KEVENT event; PSCATTER_GATHER_LIST sg; };
static void map_done(PDEVICE_OBJECT dev,PIRP irp,PSCATTER_GATHER_LIST sg,PVOID arg)
{
    struct map_wait *w=(struct map_wait *)arg;
    UNREFERENCED_PARAMETER(dev);UNREFERENCED_PARAMETER(irp);
    w->sg=sg;KeSetEvent(&w->event,IO_NO_INCREMENT,FALSE);
}
static NTSTATUS map_one(PDMA_ADAPTER adapter,PDEVICE_OBJECT pdo,PMDL mdl,
    BOOLEAN write_to_device,PSCATTER_GATHER_LIST *out)
{
    struct map_wait w;NTSTATUS status;
    KeInitializeEvent(&w.event,NotificationEvent,FALSE);w.sg=NULL;
    status=adapter->DmaOperations->GetScatterGatherList(adapter,pdo,mdl,
        MmGetMdlVirtualAddress(mdl),A04_SHARED_SCANOUT_BYTES,map_done,&w,write_to_device);
    if (!NT_SUCCESS(status))return status;
    do {status=KeWaitForSingleObject(&w.event,Executive,KernelMode,FALSE,NULL);} while(status!=STATUS_SUCCESS);
    if(!w.sg)return STATUS_DEVICE_CONFIGURATION_ERROR;
    *out=w.sg;return STATUS_SUCCESS;
}
#endif
static NTSTATUS display_address(PSCATTER_GATHER_LIST sg,uint32_t *out)
{
    uint64_t base=0,next=0,remaining=A04_SHARED_SCANOUT_BYTES;ULONG i;
    if(!sg || !out || !sg->NumberOfElements)return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<sg->NumberOfElements;i++) {
        uint64_t a=(uint64_t)sg->Elements[i].Address.QuadPart;
        ULONG len=sg->Elements[i].Length;
        if(!len || len>remaining || a>UINT32_MAX || len-1u>UINT32_MAX-a || (i && a!=next))
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        if(!i)base=a;
        next=a+len;remaining-=len;
    }
    if(remaining || !base || (base&3u))return STATUS_DEVICE_CONFIGURATION_ERROR;
    *out=(uint32_t)base;return STATUS_SUCCESS;
}
#if !defined(A04_SHARED_SCANOUT_VIDMM_ONLY) || !A04_SHARED_SCANOUT_VIDMM_ONLY
NTSTATUS a04_shared_scanout_handoff_display(struct a04_shared_scanout *s,
    PDEVICE_OBJECT display_pdo,a04_shared_quiesce_proof proof,void *ctx)
{
    NTSTATUS status;PSCATTER_GATHER_LIST fresh=NULL;uint32_t address=0;
    if(!s || !display_pdo || !proof || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
       !s->exposed || !s->mdl || !s->gpu_sg || !s->display_sg ||
       !s->gpu_adapter || !s->display_adapter)return STATUS_INVALID_DEVICE_STATE;
    /* Proof is an ownership operation, not an IRQ observation or GPU fence. */
    status=proof(ctx);if(!NT_SUCCESS(status))return status;
    s->display_dma=0;
    s->display_adapter->DmaOperations->PutScatterGatherList(s->display_adapter,s->display_sg,TRUE);
    s->display_sg=NULL;
    s->gpu_adapter->DmaOperations->PutScatterGatherList(s->gpu_adapter,s->gpu_sg,FALSE);
    s->gpu_sg=NULL;
    status=map_one(s->display_adapter,display_pdo,s->mdl,TRUE,&fresh);
    if(!NT_SUCCESS(status))return status;
    status=display_address(fresh,&address);
    if(!NT_SUCCESS(status)) {
        s->display_adapter->DmaOperations->PutScatterGatherList(s->display_adapter,fresh,TRUE);
        return status;
    }
    s->display_sg=fresh;s->display_dma=address;return STATUS_SUCCESS;
}
NTSTATUS a04_shared_scanout_release(struct a04_shared_scanout *s)
{
    if (!s || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if (s->exposed || s->display_common)return STATUS_DEVICE_BUSY;
    if (s->display_sg) s->display_adapter->DmaOperations->PutScatterGatherList(s->display_adapter,s->display_sg,TRUE);
    if (s->gpu_sg) s->gpu_adapter->DmaOperations->PutScatterGatherList(s->gpu_adapter,s->gpu_sg,FALSE);
    if (s->gpu_pages) ExFreePoolWithTag(s->gpu_pages,TAG);
    if (s->mdl) {MmFreePagesFromMdl(s->mdl);ExFreePool(s->mdl);}
    RtlZeroMemory(s,sizeof(*s));return STATUS_SUCCESS;
}
NTSTATUS a04_shared_scanout_prepare(struct a04_shared_scanout *s,
    PDMA_ADAPTER gpu,PDEVICE_OBJECT gpu_pdo,PDMA_ADAPTER display,PDEVICE_OBJECT display_pdo)
{
    PHYSICAL_ADDRESS low,high,skip;struct a04_gpu_dma_segment *segments=NULL;
    ULONG i,count;NTSTATUS status=STATUS_INVALID_PARAMETER;
    if(!s || !gpu || !display || gpu==display || !gpu_pdo || !display_pdo || gpu_pdo==display_pdo ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || s->mdl || s->gpu_sg || s->display_sg || s->gpu_pages || s->exposed ||
       !gpu->DmaOperations || !display->DmaOperations ||
       !gpu->DmaOperations->GetScatterGatherList || !gpu->DmaOperations->PutScatterGatherList ||
       !display->DmaOperations->GetScatterGatherList || !display->DmaOperations->PutScatterGatherList)
        return status;
    s->gpu_adapter=gpu;s->display_adapter=display;
    low.QuadPart=0;high.QuadPart=0xffffffffLL;skip.QuadPart=0;
    s->mdl=MmAllocatePagesForMdlEx(low,high,skip,A04_SHARED_SCANOUT_BYTES,MmNonCached,MM_ALLOCATE_FULLY_REQUIRED);
    if(!s->mdl){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    if(MmGetMdlByteOffset(s->mdl)!=0 || MmGetMdlByteCount(s->mdl)!=A04_SHARED_SCANOUT_BYTES){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    /* GPU writes system memory; OVL reads it. */
    status=map_one(gpu,gpu_pdo,s->mdl,FALSE,&s->gpu_sg);if(!NT_SUCCESS(status))goto fail;
    status=map_one(display,display_pdo,s->mdl,TRUE,&s->display_sg);if(!NT_SUCCESS(status))goto fail;
    count=s->gpu_sg->NumberOfElements;
    if(!count || count>8192u){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    segments=ExAllocatePoolWithTag(NonPagedPoolNx,(SIZE_T)count*sizeof(*segments),TAG);
    s->gpu_pages=ExAllocatePoolWithTag(NonPagedPoolNx,A04_SHARED_SCANOUT_PAGES*sizeof(*s->gpu_pages),TAG);
    if(!segments || !s->gpu_pages){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    for(i=0;i<count;i++){segments[i].address=(uint64_t)s->gpu_sg->Elements[i].Address.QuadPart;segments[i].length=s->gpu_sg->Elements[i].Length;}
    if(a04_gpu_dma_pages(segments,count,s->gpu_pages,A04_SHARED_SCANOUT_PAGES)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    for(i=0;i<A04_SHARED_SCANOUT_PAGES;i++) {
        ULONG j;
        for(j=0;j<i;j++)if(s->gpu_pages[i]==s->gpu_pages[j]) {
            status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;
        }
    }
    status=display_address(s->display_sg,&s->display_dma);
    if(!NT_SUCCESS(status))goto fail;
    ExFreePoolWithTag(segments,TAG);return STATUS_SUCCESS;
fail:
    if(segments)ExFreePoolWithTag(segments,TAG);
    (void)a04_shared_scanout_release(s);return status;
}

NTSTATUS a04_shared_scanout_build_tables(const struct a04_shared_scanout *s,
    uint32_t *pc,uint64_t pc_dma,uint64_t *pd,uint64_t pd_dma,
    uint64_t *pt0,uint64_t pt0_dma,uint64_t *pt1,uint64_t pt1_dma)
{
    struct a04_gpu_mmu_leaf leaves[2];
    uint64_t table_dma[4];ULONG i,j;
    if (!s || !s->mdl || !s->gpu_sg || !a04_shared_has_display_backing(s) || !s->gpu_pages || s->exposed ||
        !pc || !pd || !pt0 || !pt1) return STATUS_INVALID_PARAMETER;
    table_dma[0]=pc_dma;table_dma[1]=pd_dma;table_dma[2]=pt0_dma;table_dma[3]=pt1_dma;
    for(i=0;i<4;i++) {
        if ((table_dma[i]&4095u) || table_dma[i]>=(UINT64_C(1)<<40))return STATUS_DEVICE_CONFIGURATION_ERROR;
        for(j=0;j<i;j++)if(table_dma[i]==table_dma[j])return STATUS_DEVICE_CONFIGURATION_ERROR;
        for(j=0;j<A04_SHARED_SCANOUT_PAGES;j++)if(table_dma[i]==s->gpu_pages[j])return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    leaves[0].cpu=pt0;leaves[0].dma=pt0_dma;
    leaves[1].cpu=pt1;leaves[1].dma=pt1_dma;
    if(a04_gpu_mmu_map_pages_multi_4k(pc,pd,pd_dma,leaves,2,UINT64_C(0x8000000000),
          s->gpu_pages,A04_SHARED_SCANOUT_PAGES,A04_GPU_MMU_SLC_BYPASS))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    return STATUS_SUCCESS;
}

#endif
NTSTATUS a04_shared_scanout_match_vidmm(PMDL mdl,PSCATTER_GATHER_LIST sg,uint32_t *out) {
 PFN_NUMBER *pfns;ULONG page=0;uint32_t base;NTSTATUS status;
 if(!mdl||!sg||!out||MmGetMdlByteOffset(mdl)||MmGetMdlByteCount(mdl)!=A04_SHARED_SCANOUT_BYTES||
    !(mdl->MdlFlags&MDL_PAGES_LOCKED)||(mdl->MdlFlags&MDL_IO_SPACE))return STATUS_INVALID_PARAMETER;
 status=display_address(sg,&base);if(!NT_SUCCESS(status))return status;
 pfns=MmGetMdlPfnArray(mdl);
 for(ULONG i=0;i<sg->NumberOfElements;i++){
  uint64_t a=sg->Elements[i].Address.QuadPart;ULONG n=sg->Elements[i].Length;
  if((a&4095u)||(n&4095u)||n/4096u>A04_SHARED_SCANOUT_PAGES-page)return STATUS_DEVICE_CONFIGURATION_ERROR;
  for(ULONG j=0;j<n/4096u;j++,page++)
   if(!pfns[page]||(uint64_t)pfns[page]>0xfffffu||a+j*4096u!=((uint64_t)pfns[page]<<12))return STATUS_DEVICE_CONFIGURATION_ERROR;
 }
 if(page!=A04_SHARED_SCANOUT_PAGES)return STATUS_DEVICE_CONFIGURATION_ERROR;
 *out=base;return STATUS_SUCCESS;
}
