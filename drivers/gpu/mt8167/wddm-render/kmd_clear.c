/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "kmd_clear.h"
static int ticket_range(UINT size,UINT begin,UINT end) {
 return begin<=size&&end>=begin&&end-begin==sizeof(struct geo_dma_ticket)&&end<=size;
}
NTSTATUS APIENTRY GeoCreateAllocation(HANDLE adapter,DXGKARG_CREATEALLOCATION *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation;struct geo_surface_desc desc;DXGK_ALLOCATIONINFO *info;NTSTATUS s;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||p->NumAllocations!=1||!p->pAllocationInfo||p->PrivateDriverDataSize)return STATUS_INVALID_PARAMETER;
 info=p->pAllocationInfo;
 if(info->PrivateDriverDataSize!=sizeof(desc)||!info->pPrivateDriverData)return STATUS_INVALID_PARAMETER;
 RtlCopyMemory(&desc,info->pPrivateDriverData,sizeof(desc));if(!geo_desc_valid(&desc))return STATUS_NOT_SUPPORTED;
 allocation=ExAllocatePoolWithTag(NonPagedPool,sizeof(*allocation),'rOeG');if(!allocation)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(allocation,sizeof(*allocation));s=geo_create(&a->engine,allocation,&desc);
 if(s!=STATUS_SUCCESS){ExFreePoolWithTag(allocation,'rOeG');return s;}
 info->hAllocation=allocation;info->Alignment=PAGE_SIZE;info->Size=ROUND_TO_PAGES(desc.pitch*desc.height);
 info->PitchAlignedSize=info->Size;info->PreferredSegment.Value=0;info->PreferredSegment.SegmentId0=1;
 info->SupportedReadSegmentSet=1;info->SupportedWriteSegmentSet=1;info->EvictionSegmentSet=0;
 info->Flags.Value=0;info->Flags.CpuVisible=1;info->Flags.Cached=1;
 info->AllocationPriority=D3DDDI_ALLOCATIONPRIORITY_NORMAL;return STATUS_SUCCESS;
}
NTSTATUS APIENTRY GeoDestroyAllocation(HANDLE adapter,const DXGKARG_DESTROYALLOCATION *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation;NTSTATUS s;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||p->NumAllocations!=1||!p->pAllocationList)return STATUS_INVALID_PARAMETER;
 allocation=p->pAllocationList[0];if(!allocation)return STATUS_INVALID_PARAMETER;
 s=geo_destroy(&a->engine,allocation);if(s!=STATUS_SUCCESS)return s;
 ExFreePoolWithTag(allocation,'rOeG');return STATUS_SUCCESS;
}
NTSTATUS APIENTRY GeoBuildPagingBuffer(HANDLE adapter,DXGKARG_BUILDPAGINGBUFFER *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation;uint32_t bytes;NTSTATUS status;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(p->Operation==DXGK_OPERATION_MAP_APERTURE_SEGMENT) {
  allocation=p->MapApertureSegment.hAllocation;
  if(!allocation||p->MapApertureSegment.SegmentId!=1||p->MapApertureSegment.Flags.Value||
     !p->MapApertureSegment.pMdl||MmGetMdlByteOffset(p->MapApertureSegment.pMdl)||p->MapApertureSegment.MdlOffset>UINT32_MAX/PAGE_SIZE)return STATUS_NOT_SUPPORTED;
  bytes=allocation->desc.pitch*allocation->desc.height;
  if(p->MapApertureSegment.NumberOfPages!=BYTES_TO_PAGES(bytes))return STATUS_NOT_SUPPORTED;
  /* Microsoft DDI defines MdlOffset in PAGES, not bytes. The provider must
   * retain this actual MDL/PFN backing and perform real CPU/GPU maintenance. */
  if((uint64_t)p->MapApertureSegment.MdlOffset*PAGE_SIZE+bytes>MmGetMdlByteCount(p->MapApertureSegment.pMdl))return STATUS_INVALID_PARAMETER;
  status=geo_bind(&a->engine,allocation,p->MapApertureSegment.pMdl,p->MapApertureSegment.MdlOffset*PAGE_SIZE,bytes);
  if(status==STATUS_SUCCESS){allocation->aperture_offset=p->MapApertureSegment.OffsetInPages;allocation->aperture_pages=(uint32_t)p->MapApertureSegment.NumberOfPages;}
  return status;
 }
 if(p->Operation==DXGK_OPERATION_UNMAP_APERTURE_SEGMENT) {
  allocation=p->UnmapApertureSegment.hAllocation;
  if(!allocation||p->UnmapApertureSegment.SegmentId!=1||
     p->UnmapApertureSegment.OffsetInPages!=allocation->aperture_offset||
     p->UnmapApertureSegment.NumberOfPages!=allocation->aperture_pages)return STATUS_INVALID_PARAMETER;
  return geo_unbind(&a->engine,allocation);
 }
 /* Fill/transfer/discard are not silently accepted without implementing them. */
 return STATUS_NOT_SUPPORTED;
}
struct prepare_call { struct geo_kmd_adapter *adapter;struct geo_allocation *allocation;struct geo_clear_packet packet;struct geo_dma_ticket ticket;uint64_t token;NTSTATUS status; };
static BOOLEAN begin_prepare(void *ctx) {
 struct prepare_call *c=ctx;c->status=geo_prepare_begin(&c->adapter->engine,c->allocation,&c->packet,&c->ticket);return TRUE;
}
static BOOLEAN finish_prepare(void *ctx) {
 struct prepare_call *c=ctx;c->status=geo_prepare_finish(&c->adapter->engine,&c->ticket,c->status,c->token);return TRUE;
}
NTSTATUS APIENTRY GeoRender(HANDLE context,DXGKARG_RENDER *r) {
 struct geo_kmd_context *c=context;struct prepare_call call;struct geo_clear_packet p;struct geo_dma_ticket t;
 struct geo_allocation *a;
 if(!c||!c->adapter||!c->adapter->synchronize||!r||KeGetCurrentIrql()!=PASSIVE_LEVEL||!r->pCommand||r->CommandLength!=sizeof(p)||
    r->DmaBufferSegmentId||r->MultipassOffset||r->AllocationListSize!=1||!r->pAllocationList||r->PatchLocationListInSize||
    !r->pDmaBuffer||r->DmaSize<sizeof(t)||!r->pDmaBufferPrivateData||r->DmaBufferPrivateDataSize<sizeof(t))return STATUS_INVALID_PARAMETER;
 __try { RtlCopyMemory(&p,r->pCommand,sizeof(p)); }
 __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
 if(!geo_packet_valid(&p,1)||!r->pAllocationList[0].WriteOperation)return STATUS_INVALID_PARAMETER;
 a=r->pAllocationList[0].hDeviceSpecificAllocation;
 /* Full CreateAllocation/DestroyAllocation owns and validates this handle.
  * VidMm provides this kernel list; UMD cannot pass a pointer in the packet. */
 if(!a)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&call,sizeof(call));call.adapter=c->adapter;call.allocation=a;call.packet=p;
 if(!c->adapter->synchronize(c->adapter->sync_context,begin_prepare,&call))return STATUS_DEVICE_NOT_READY;
 if(call.status!=STATUS_SUCCESS)return call.status;
 call.status=c->adapter->engine.owner.prepare(c->adapter->engine.owner.context,a->surface,p.color,&call.token);
 if(!c->adapter->synchronize(c->adapter->sync_context,finish_prepare,&call))return STATUS_DEVICE_NOT_READY;
 if(call.status!=STATUS_SUCCESS)return call.status;t=call.ticket;
 RtlCopyMemory(r->pDmaBuffer,&t,sizeof(t));RtlCopyMemory(r->pDmaBufferPrivateData,&t,sizeof(t));
 r->pDmaBuffer=(UCHAR *)r->pDmaBuffer+sizeof(t);
 r->pDmaBufferPrivateData=(UCHAR *)r->pDmaBufferPrivateData+sizeof(t);
 return STATUS_SUCCESS;
}
NTSTATUS APIENTRY GeoPatch(HANDLE adapter,const DXGKARG_PATCH *p) {
 struct geo_dma_ticket t;UNREFERENCED_PARAMETER(adapter);
 if(!p||p->EngineOrdinal||p->DmaBufferSegmentId||p->Flags.Value||p->PatchLocationListSubmissionLength||
    !p->pDmaBuffer||!p->pDmaBufferPrivateData||
    !ticket_range(p->DmaBufferSize,p->DmaBufferSubmissionStartOffset,p->DmaBufferSubmissionEndOffset)||
    !ticket_range(p->DmaBufferPrivateDataSize,p->DmaBufferPrivateDataSubmissionStartOffset,p->DmaBufferPrivateDataSubmissionEndOffset))return STATUS_NOT_SUPPORTED;
 RtlCopyMemory(&t,(UCHAR *)p->pDmaBuffer+p->DmaBufferSubmissionStartOffset,sizeof(t));
 if(t.magic!=GEO_TICKET_MAGIC||t.reserved||t.slot>=GEO_MAX_JOBS||
    RtlCompareMemory(&t,(UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,sizeof(t))!=sizeof(t))return STATUS_INVALID_PARAMETER;
 /* A semantic ticket has no addresses to patch. No fake GPU DMA packet. */
 return STATUS_SUCCESS;
}
struct submit_call { struct geo_kmd_adapter *adapter;struct geo_dma_ticket ticket;uint32_t fence;NTSTATUS status; };
static BOOLEAN submit_locked(void *ctx) {
 struct submit_call *c=ctx;c->status=geo_submit(&c->adapter->engine,&c->ticket,c->fence);return TRUE;
}
NTSTATUS APIENTRY GeoSubmitCommand(HANDLE adapter,const DXGKARG_SUBMITCOMMAND *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_dma_ticket t;struct submit_call call;
 if(!a||!a->synchronize||!p||KeGetCurrentIrql()>DISPATCH_LEVEL||p->EngineOrdinal||p->NodeOrdinal||p->DmaBufferSegmentId||
    p->Flags.Value||!p->pDmaBufferPrivateData||
    !ticket_range(p->DmaBufferPrivateDataSize,p->DmaBufferPrivateDataSubmissionStartOffset,p->DmaBufferPrivateDataSubmissionEndOffset))return STATUS_NOT_SUPPORTED;
 RtlCopyMemory(&t,(UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,sizeof(t));
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;call.ticket=t;call.fence=p->SubmissionFenceId;
 if(!a->synchronize(a->sync_context,submit_locked,&call))return STATUS_DEVICE_NOT_READY;
 return call.status;
}
struct fence_call { struct geo_kmd_adapter *adapter;uint32_t fence;NTSTATUS status; };
static BOOLEAN query_fence_locked(void *ctx) {
 struct fence_call *c=ctx;
 c->status=c->adapter->engine.poisoned?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;
 c->fence=c->adapter->engine.completed_ever?c->adapter->engine.last_completed:0;return TRUE;
}
NTSTATUS APIENTRY GeoQueryCurrentFence(HANDLE adapter,DXGKARG_QUERYCURRENTFENCE *p) {
 struct geo_kmd_adapter *a=adapter;struct fence_call call;
 if(!a||!a->synchronize||!p||p->NodeOrdinal||p->EngineOrdinal||KeGetCurrentIrql()>DISPATCH_LEVEL)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;
 if(!a->synchronize(a->sync_context,query_fence_locked,&call))return STATUS_DEVICE_NOT_READY;
 if(call.status==STATUS_SUCCESS)p->CurrentFence=call.fence;return call.status;
}
