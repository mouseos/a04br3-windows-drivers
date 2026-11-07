/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "kmd_clear.h"
#include "capture.h"
#include "lifecycle.h"
#include "kmd_objects.h"
static int ticket_range(UINT size,UINT begin,UINT end) {
 return begin<=size&&end>=begin&&end-begin==sizeof(struct geo_dma_ticket)&&end<=size;
}
static NTSTATUS GeoCreateAllocationImpl(HANDLE adapter,DXGKARG_CREATEALLOCATION *p) {
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
static NTSTATUS GeoDestroyAllocationImpl(HANDLE adapter,const DXGKARG_DESTROYALLOCATION *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation;NTSTATUS s;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||p->NumAllocations!=1||!p->pAllocationList)return STATUS_INVALID_PARAMETER;
 allocation=p->pAllocationList[0];if(!allocation)return STATUS_INVALID_PARAMETER;
 s=geo_destroy(&a->engine,allocation);if(s!=STATUS_SUCCESS)return s;
 ExFreePoolWithTag(allocation,'rOeG');return STATUS_SUCCESS;
}
static NTSTATUS GeoBuildPagingBufferImpl(HANDLE adapter,DXGKARG_BUILDPAGINGBUFFER *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation;uint32_t bytes;NTSTATUS status;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(p->Operation==DXGK_OPERATION_MAP_APERTURE_SEGMENT) {
  allocation=p->MapApertureSegment.hAllocation;
  if(!allocation||p->MapApertureSegment.SegmentId!=1||p->MapApertureSegment.Flags.Value||
     !p->MapApertureSegment.pMdl||MmGetMdlByteOffset(p->MapApertureSegment.pMdl)||p->MapApertureSegment.MdlOffset)return STATUS_NOT_SUPPORTED;
  bytes=allocation->desc.pitch*allocation->desc.height;
  if(p->MapApertureSegment.NumberOfPages!=BYTES_TO_PAGES(bytes))return STATUS_NOT_SUPPORTED;
  /* Microsoft DDI defines MdlOffset in PAGES, not bytes. The provider must
   * retain this actual MDL/PFN backing and perform real CPU/GPU maintenance. */
  if(ROUND_TO_PAGES(bytes)!=MmGetMdlByteCount(p->MapApertureSegment.pMdl))return STATUS_INVALID_PARAMETER;
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
struct prepare_call { struct geo_kmd_adapter *adapter;struct geo_allocation *allocation;struct geo_clear_packet packet;struct geo_kmd_context *context;HANDLE open_handle;struct geo_dma_ticket ticket;uint64_t token;NTSTATUS status; };
static BOOLEAN begin_prepare(void *ctx) {
 struct prepare_call *c=ctx;c->status=geo_prepare_begin(&c->adapter->engine,c->allocation,&c->packet,&c->ticket);
 if(c->status==STATUS_SUCCESS){c->adapter->engine.jobs[c->ticket.slot].context=c->context;c->adapter->engine.jobs[c->ticket.slot].open_handle=c->open_handle;}return TRUE;
}
static BOOLEAN finish_prepare(void *ctx) {
 struct prepare_call *c=ctx;c->status=geo_prepare_finish(&c->adapter->engine,&c->ticket,c->status,c->token);return TRUE;
}
static NTSTATUS GeoRenderImpl(HANDLE context,DXGKARG_RENDER *r) {
 struct geo_kmd_context *c=context;struct prepare_call call;struct geo_clear_packet p;struct geo_dma_ticket t;
 struct geo_allocation *a;NTSTATUS capture_status;
 if(!c||!c->adapter||!c->adapter->notifications_ready||!c->adapter->synchronize||!r||KeGetCurrentIrql()!=PASSIVE_LEVEL||!r->pCommand||r->CommandLength!=sizeof(p)||
    r->DmaBufferSegmentId||r->MultipassOffset||r->AllocationListSize!=1||!r->pAllocationList||r->PatchLocationListInSize||!r->pPatchLocationListOut||r->PatchLocationListOutSize<1||
    !r->pDmaBuffer||r->DmaSize<sizeof(t)||!r->pDmaBufferPrivateData||r->DmaBufferPrivateDataSize<sizeof(t))return STATUS_INVALID_PARAMETER;
 capture_status=GeoCaptureClearPacket(&p,r->pCommand);
 if(capture_status!=STATUS_SUCCESS)return capture_status;
 if(!geo_packet_valid(&p,1)||!r->pAllocationList[0].WriteOperation)return STATUS_INVALID_PARAMETER;
 a=GeoResolveWriteAllocation(c,r->pAllocationList[0].hDeviceSpecificAllocation);
 /* Full CreateAllocation/DestroyAllocation owns and validates this handle.
  * VidMm provides this kernel list; UMD cannot pass a pointer in the packet. */
 if(!a)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&call,sizeof(call));call.adapter=c->adapter;call.allocation=a;call.packet=p;call.context=c;call.open_handle=r->pAllocationList[0].hDeviceSpecificAllocation;
 if(!c->adapter->synchronize(c->adapter->sync_context,begin_prepare,&call))return STATUS_DEVICE_NOT_READY;
 if(call.status!=STATUS_SUCCESS)return call.status;
 call.status=c->adapter->engine.owner.prepare(c->adapter->engine.owner.context,a->surface,p.color,&call.token);
 if(!c->adapter->synchronize(c->adapter->sync_context,finish_prepare,&call))return STATUS_DEVICE_NOT_READY;
 if(call.status!=STATUS_SUCCESS)return call.status;t=call.ticket;
 RtlCopyMemory(r->pDmaBuffer,&t,sizeof(t));RtlCopyMemory(r->pDmaBufferPrivateData,&t,sizeof(t));
 r->pDmaBuffer=(UCHAR *)r->pDmaBuffer+sizeof(t);
 r->pDmaBufferPrivateData=(UCHAR *)r->pDmaBufferPrivateData+sizeof(t);
 /* Semantic residency reference. No physical address is stored in a ticket,
  * but VidMm must retain/track the actual referenced allocation. */
 RtlZeroMemory(r->pPatchLocationListOut,sizeof(*r->pPatchLocationListOut));
 r->pPatchLocationListOut->AllocationIndex=0;r->pPatchLocationListOut->DriverId=GEO_CLEAR_MAGIC;
 r->pPatchLocationListOut++;return STATUS_SUCCESS;
}
struct inspect_ticket { struct geo_kmd_adapter *adapter;struct geo_dma_ticket ticket;HANDLE context,open_handle;NTSTATUS status; };
static BOOLEAN inspect_prepared(void *arg) {
 struct inspect_ticket *c=arg;struct geo_job *j;
 c->status=STATUS_INVALID_PARAMETER;
 if(c->ticket.magic!=GEO_TICKET_MAGIC||c->ticket.reserved||c->ticket.slot>=GEO_MAX_JOBS)return TRUE;
 j=&c->adapter->engine.jobs[c->ticket.slot];
 if(!c->adapter->engine.poisoned&&j->state==1&&j->generation==c->ticket.generation&&
    j->context==c->context&&(!c->open_handle||j->open_handle==c->open_handle))c->status=STATUS_SUCCESS;
 return TRUE;
}
static NTSTATUS GeoPatchImpl(HANDLE adapter,const DXGKARG_PATCH *p) {
 struct geo_dma_ticket t;struct geo_kmd_adapter *a=adapter;struct inspect_ticket inspect;
 const D3DDDI_PATCHLOCATIONLIST *location;
 if(!p||!p->hContext||!a||!a->synchronize||p->EngineOrdinal||p->DmaBufferSegmentId||p->Flags.Value||p->PatchLocationListSubmissionLength!=1||!p->pPatchLocationList||
    p->PatchLocationListSubmissionStart>=p->PatchLocationListSize||p->AllocationListSize!=1||!p->pAllocationList||
    !p->pDmaBuffer||!p->pDmaBufferPrivateData||
    !ticket_range(p->DmaBufferSize,p->DmaBufferSubmissionStartOffset,p->DmaBufferSubmissionEndOffset)||
    !ticket_range(p->DmaBufferPrivateDataSize,p->DmaBufferPrivateDataSubmissionStartOffset,p->DmaBufferPrivateDataSubmissionEndOffset))return STATUS_NOT_SUPPORTED;
 location=&p->pPatchLocationList[p->PatchLocationListSubmissionStart];
 if(location->AllocationIndex||location->Value||location->DriverId!=GEO_CLEAR_MAGIC||location->AllocationOffset||location->PatchOffset||location->SplitOffset)return STATUS_INVALID_PARAMETER;
 RtlCopyMemory(&t,(UCHAR *)p->pDmaBuffer+p->DmaBufferSubmissionStartOffset,sizeof(t));
 if(t.magic!=GEO_TICKET_MAGIC||t.reserved||t.slot>=GEO_MAX_JOBS||
    RtlCompareMemory(&t,(UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,sizeof(t))!=sizeof(t))return STATUS_INVALID_PARAMETER;
 /* No address is patched, but the exact generation/context/allocation
  * reference must still match the prepared job whose MDL is retained. */
 if(!p->pAllocationList[0].WriteOperation)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&inspect,sizeof(inspect));inspect.adapter=a;inspect.ticket=t;inspect.context=p->hContext;
 inspect.open_handle=p->pAllocationList[0].hDeviceSpecificAllocation;
 if(!inspect.open_handle||!a->synchronize(a->sync_context,inspect_prepared,&inspect))return STATUS_DEVICE_NOT_READY;
 return inspect.status;
}
struct submit_call { struct geo_kmd_adapter *adapter;struct geo_dma_ticket ticket;uint32_t fence;HANDLE context;NTSTATUS status; };
static BOOLEAN submit_locked(void *ctx) {
 struct submit_call *c=ctx;struct inspect_ticket inspect;
 RtlZeroMemory(&inspect,sizeof(inspect));inspect.adapter=c->adapter;inspect.ticket=c->ticket;inspect.context=c->context;
 inspect_prepared(&inspect);if(inspect.status!=STATUS_SUCCESS){c->status=inspect.status;return TRUE;}
 c->status=geo_submit(&c->adapter->engine,&c->ticket,c->fence);return TRUE;
}
static NTSTATUS GeoSubmitCommandImpl(HANDLE adapter,const DXGKARG_SUBMITCOMMAND *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_dma_ticket t;struct submit_call call;
 if(!a||!a->notifications_ready||!a->synchronize||!p||KeGetCurrentIrql()>DISPATCH_LEVEL||p->EngineOrdinal||p->NodeOrdinal||p->DmaBufferSegmentId||
    p->Flags.Value||!p->pDmaBufferPrivateData||
    !ticket_range(p->DmaBufferPrivateDataSize,p->DmaBufferPrivateDataSubmissionStartOffset,p->DmaBufferPrivateDataSubmissionEndOffset))return STATUS_NOT_SUPPORTED;
 RtlCopyMemory(&t,(UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,sizeof(t));
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;call.ticket=t;call.fence=p->SubmissionFenceId;call.context=p->hContext;
 if(!a->synchronize(a->sync_context,submit_locked,&call))return STATUS_DEVICE_NOT_READY;
 return call.status;
}
struct fence_call { struct geo_kmd_adapter *adapter;uint32_t fence;NTSTATUS status; };
static BOOLEAN query_fence_locked(void *ctx) {
 struct fence_call *c=ctx;
 c->status=c->adapter->engine.poisoned?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;
 c->fence=c->adapter->engine.completed_ever?c->adapter->engine.last_completed:0;return TRUE;
}
static NTSTATUS GeoQueryCurrentFenceImpl(HANDLE adapter,DXGKARG_QUERYCURRENTFENCE *p) {
 struct geo_kmd_adapter *a=adapter;struct fence_call call;
 if(!a||!a->notifications_ready||!a->synchronize||!p||p->NodeOrdinal||p->EngineOrdinal||KeGetCurrentIrql()>DISPATCH_LEVEL)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;
 if(!a->synchronize(a->sync_context,query_fence_locked,&call))return STATUS_DEVICE_NOT_READY;
 if(call.status==STATUS_SUCCESS)p->CurrentFence=call.fence;return call.status;
}

/* These helpers supply the scheduler half of completion forwarding; they do
 * not create an interrupt, poll hardware or fabricate a provider event. */
NTSTATUS GeoInitializeClearNotifications(struct geo_kmd_adapter *a,const DXGKRNL_INTERFACE *dxgk,KIRQL owner_irql) {
 if(!a||!dxgk||KeGetCurrentIrql()!=PASSIVE_LEVEL||a->notifications_ready||
    owner_irql<=DISPATCH_LEVEL||owner_irql>HIGH_LEVEL||dxgk->Size<sizeof(*dxgk)||
    !dxgk->DeviceHandle||!dxgk->DxgkCbGetHandleData||!dxgk->DxgkCbNotifyInterrupt||!dxgk->DxgkCbQueueDpc||!dxgk->DxgkCbNotifyDpc)return STATUS_INVALID_PARAMETER;
 /* Caller owns a zeroed, not-yet-published adapter and live StartDevice/D0
  * interface. Publication to the actual ISR occurs only after this returns. */
 a->get_handle_data=dxgk->DxgkCbGetHandleData;
 a->dxgk_device=dxgk->DeviceHandle;a->notify_interrupt=dxgk->DxgkCbNotifyInterrupt;
 a->queue_dpc=dxgk->DxgkCbQueueDpc;a->notify_dpc=dxgk->DxgkCbNotifyDpc;
 a->owner_irql=owner_irql;a->dpc_pending=0;a->notifications_ready=1;return STATUS_SUCCESS;
}
NTSTATUS GeoCompleteFromInterrupt(struct geo_kmd_adapter *a,uint64_t token,uint32_t generation,int32_t status,uint32_t visibility) {
 DXGKARGCB_NOTIFY_INTERRUPT_DATA data;uint32_t fence;NTSTATUS s;
 if(!a||!a->notifications_ready||KeGetCurrentIrql()!=a->owner_irql||a->owner_irql<=DISPATCH_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 s=geo_complete(&a->engine,token,generation,status,visibility,&fence);
 if(s!=STATUS_SUCCESS)return s; /* No stale/fault/visibility failure becomes a completed fence. */
 RtlZeroMemory(&data,sizeof(data));data.InterruptType=DXGK_INTERRUPT_DMA_COMPLETED;
 data.DmaCompleted.SubmissionFenceId=fence;data.DmaCompleted.NodeOrdinal=0;data.DmaCompleted.EngineOrdinal=0;
 a->notify_interrupt(a->dxgk_device,&data);
 InterlockedExchange(&a->dpc_pending,1);
 /* QueueDpc may return FALSE when a DPC is already queued. The registered
  * DPC wrapper must always call GeoClearDpc; no timer substitutes for it. */
 a->queue_dpc(a->dxgk_device);return STATUS_SUCCESS;
}
void GeoClearDpc(struct geo_kmd_adapter *a) {
 if(!a||!a->notifications_ready||KeGetCurrentIrql()!=DISPATCH_LEVEL)return;
 if(InterlockedExchange(&a->dpc_pending,0))a->notify_dpc(a->dxgk_device);
}

NTSTATUS APIENTRY GeoCreateAllocation(HANDLE handle,DXGKARG_CREATEALLOCATION *p) {
 struct geo_kmd_adapter *a=handle;NTSTATUS status;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 status=GeoCreateAllocationImpl(a,p);GeoSessionLeave(a,1);return status;
}
NTSTATUS APIENTRY GeoDestroyAllocation(HANDLE handle,const DXGKARG_DESTROYALLOCATION *p) {
 struct geo_kmd_adapter *a=handle;NTSTATUS status;
 if(!GeoSessionEnter(a,2))return STATUS_DEVICE_NOT_READY;
 status=GeoDestroyAllocationImpl(a,p);GeoSessionLeave(a,2);return status;
}
NTSTATUS APIENTRY GeoBuildPagingBuffer(HANDLE handle,DXGKARG_BUILDPAGINGBUFFER *p) {
 struct geo_kmd_adapter *a=handle;NTSTATUS status;UINT mode;
 if(!p)return STATUS_INVALID_PARAMETER;
 mode=p->Operation==DXGK_OPERATION_UNMAP_APERTURE_SEGMENT?2:1;
 if(!GeoSessionEnter(a,mode))return STATUS_DEVICE_NOT_READY;
 status=GeoBuildPagingBufferImpl(a,p);GeoSessionLeave(a,mode);return status;
}
NTSTATUS APIENTRY GeoRender(HANDLE handle,DXGKARG_RENDER *p) {
 struct geo_kmd_context *c=handle;NTSTATUS status;
 if(!c||!GeoSessionEnter(c->adapter,1))return STATUS_DEVICE_NOT_READY;
 status=GeoRenderImpl(c,p);GeoSessionLeave(c->adapter,1);return status;
}
NTSTATUS APIENTRY GeoSubmitCommand(HANDLE handle,const DXGKARG_SUBMITCOMMAND *p) {
 struct geo_kmd_adapter *a=handle;NTSTATUS status;
 if(!GeoSessionEnter(a,0))return STATUS_DEVICE_NOT_READY;
 status=GeoSubmitCommandImpl(a,p);GeoSessionLeave(a,0);return status;
}
NTSTATUS APIENTRY GeoQueryCurrentFence(HANDLE handle,DXGKARG_QUERYCURRENTFENCE *p) {
 struct geo_kmd_adapter *a=handle;NTSTATUS status;
 if(!GeoSessionEnter(a,0))return STATUS_DEVICE_NOT_READY;
 status=GeoQueryCurrentFenceImpl(a,p);GeoSessionLeave(a,0);return status;
}

NTSTATUS APIENTRY GeoPatch(HANDLE handle,const DXGKARG_PATCH *p) {
 struct geo_kmd_adapter *a=handle;NTSTATUS status;
 if(!GeoSessionEnter(a,0))return STATUS_DEVICE_NOT_READY;
 status=GeoPatchImpl(a,p);GeoSessionLeave(a,0);return status;
}
