/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "kmd_clear.h"
#include "capture.h"
#include "lifecycle.h"
#include "kmd_objects.h"
#include "ticket_io.h"
static int ticket_range(UINT size,UINT begin,UINT end) {
 return begin<=size&&end>=begin&&end-begin==sizeof(struct geo_dma_ticket)&&end<=size;
}
static NTSTATUS GeoCreateAllocationImpl(HANDLE adapter,DXGKARG_CREATEALLOCATION *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation;struct geo_surface_desc desc;struct geo_allocation_data data;DXGK_ALLOCATIONINFO *info;NTSTATUS s;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||p->NumAllocations!=1||!p->pAllocationInfo||p->PrivateDriverDataSize)return STATUS_INVALID_PARAMETER;
 info=p->pAllocationInfo;
 if(info->PrivateDriverDataSize!=sizeof(data)||!info->pPrivateDriverData)return STATUS_INVALID_PARAMETER;
 RtlCopyMemory(&data,info->pPrivateDriverData,sizeof(data));
 if(data.size!=sizeof(data)||data.version!=1u||!data.logical_width||!data.logical_height)return STATUS_INVALID_PARAMETER;
 desc=data.storage;
 if(desc.usage==GEO_USAGE_RENDER_TARGET){
  if(desc.width!=800u||desc.height!=600u||desc.pitch!=3200u||data.logical_width>800u||data.logical_height>600u)return STATUS_NOT_SUPPORTED;
 }else if(data.logical_width!=desc.width||data.logical_height!=desc.height)return STATUS_INVALID_PARAMETER;
if(!a->aperture_ready||!geo_desc_valid(&desc)||ROUND_TO_PAGES(desc.pitch*desc.height)>a->aperture.commit_limit)return STATUS_NOT_SUPPORTED;
 allocation=ExAllocatePoolWithTag(NonPagedPool,sizeof(*allocation),'rOeG');if(!allocation)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(allocation,sizeof(*allocation));s=geo_create(&a->engine,allocation,&desc);
 if(s!=STATUS_SUCCESS){ExFreePoolWithTag(allocation,'rOeG');return s;}
 allocation->logical_width=data.logical_width;allocation->logical_height=data.logical_height;
 info->hAllocation=allocation;info->Alignment=PAGE_SIZE;info->Size=ROUND_TO_PAGES(desc.pitch*desc.height);
 info->PitchAlignedSize=info->Size;info->PreferredSegment.Value=0;info->PreferredSegment.SegmentId0=1;
 info->SupportedReadSegmentSet=allocation->cpu_only?0:1;info->SupportedWriteSegmentSet=allocation->cpu_only?0:1;
 if(allocation->cpu_only)info->PreferredSegment.Value=0;info->EvictionSegmentSet=0;
 info->Flags.Value=0;info->Flags.CpuVisible=1;info->Flags.Cached=1;info->Flags.SynchronousPaging=1;
 info->AllocationPriority=D3DDDI_ALLOCATIONPRIORITY_NORMAL;return STATUS_SUCCESS;
}
static NTSTATUS GeoDestroyAllocationImpl(HANDLE adapter,const DXGKARG_DESTROYALLOCATION *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation;NTSTATUS s;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||p->NumAllocations!=1||!p->pAllocationList)return STATUS_INVALID_PARAMETER;
 allocation=p->pAllocationList[0];if(!allocation)return STATUS_INVALID_PARAMETER;
 s=geo_destroy(&a->engine,allocation);if(s!=STATUS_SUCCESS)return s;
 GeoEndVidMmMap(a,allocation);
 ExFreePoolWithTag(allocation,'rOeG');return STATUS_SUCCESS;
}
static NTSTATUS GeoBuildPagingBufferImpl(HANDLE adapter,DXGKARG_BUILDPAGINGBUFFER *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation,*scan;uint32_t bytes;uint64_t begin,extent,other;NTSTATUS status;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(p->Operation==DXGK_OPERATION_MAP_APERTURE_SEGMENT) {
  allocation=p->MapApertureSegment.hAllocation;
  if(!allocation||p->MapApertureSegment.SegmentId!=1||p->MapApertureSegment.Flags.Value||
     !p->MapApertureSegment.pMdl||MmGetMdlByteOffset(p->MapApertureSegment.pMdl)||p->MapApertureSegment.MdlOffset)return STATUS_NOT_SUPPORTED;
  if(allocation->cpu_only)return STATUS_NOT_SUPPORTED;
  bytes=allocation->desc.pitch*allocation->desc.height;
  if(p->MapApertureSegment.NumberOfPages!=BYTES_TO_PAGES(bytes))return STATUS_NOT_SUPPORTED;
  if(!a->aperture_ready)return STATUS_DEVICE_NOT_READY;
  begin=(uint64_t)p->MapApertureSegment.OffsetInPages*PAGE_SIZE;extent=ROUND_TO_PAGES(bytes);
  if(begin>a->aperture.bytes||extent>a->aperture.bytes-begin)return STATUS_INVALID_PARAMETER;
  for(scan=a->engine.allocations;scan;scan=scan->next)if(scan->bound) {
   other=scan->aperture_offset*PAGE_SIZE;
   if(begin<other+(uint64_t)scan->aperture_pages*PAGE_SIZE&&other<begin+extent)return STATUS_DEVICE_BUSY;
  }
  /* Microsoft DDI defines MdlOffset in PAGES, not bytes. The provider must
   * retain this actual MDL/PFN backing and perform real CPU/GPU maintenance. */
  if(ROUND_TO_PAGES(bytes)!=MmGetMdlByteCount(p->MapApertureSegment.pMdl))return STATUS_INVALID_PARAMETER;
  status=GeoBeginVidMmMap(a,allocation,p->MapApertureSegment.pMdl,bytes);if(status!=STATUS_SUCCESS)return status;
  status=geo_bind(&a->engine,allocation,p->MapApertureSegment.pMdl,p->MapApertureSegment.MdlOffset*PAGE_SIZE,bytes,(uint64_t)p->MapApertureSegment.OffsetInPages*PAGE_SIZE);
  if(status==STATUS_SUCCESS) {
   status=GeoConfirmVidMmBind(a,allocation);
   if(status!=STATUS_SUCCESS){allocation->poisoned=1;a->engine.poisoned=1;return status;}
  }
  /* Failed/ambiguous native bind keeps the independent physical lock. */
  if(status==STATUS_SUCCESS){allocation->aperture_offset=p->MapApertureSegment.OffsetInPages;allocation->aperture_pages=(uint32_t)p->MapApertureSegment.NumberOfPages;}
  return status;
 }
 if(p->Operation==DXGK_OPERATION_UNMAP_APERTURE_SEGMENT) {
  allocation=p->UnmapApertureSegment.hAllocation;
  if(!allocation||p->UnmapApertureSegment.SegmentId!=1||
     p->UnmapApertureSegment.OffsetInPages!=allocation->aperture_offset||
     p->UnmapApertureSegment.NumberOfPages!=allocation->aperture_pages)return STATUS_INVALID_PARAMETER;
  status=geo_unbind(&a->engine,allocation);
  if(status==STATUS_SUCCESS)GeoEndVidMmMap(a,allocation);return status;
 }
 /* Fill/transfer/discard are not silently accepted without implementing them. */
 return STATUS_NOT_SUPPORTED;
}
struct prepare_call { struct geo_kmd_adapter *adapter;struct geo_allocation *allocation,*vertex;struct geo_clear_packet packet;struct geo_kmd_context *context;HANDLE open_handle,vertex_open_handle;struct geo_dma_ticket ticket;uint64_t token;NTSTATUS status; };
static BOOLEAN begin_prepare(void *ctx) {
 struct prepare_call *c=ctx;c->status=c->vertex?geo_prepare_draw_begin(&c->adapter->engine,c->allocation,c->vertex,&c->ticket):geo_prepare_begin(&c->adapter->engine,c->allocation,&c->packet,&c->ticket);
 if(c->status==STATUS_SUCCESS){c->adapter->engine.jobs[c->ticket.slot].context=c->context;c->adapter->engine.jobs[c->ticket.slot].open_handle=c->open_handle;c->adapter->engine.jobs[c->ticket.slot].vertex_open_handle=c->vertex_open_handle;}return TRUE;
}
static BOOLEAN finish_prepare(void *ctx) {
 struct prepare_call *c=ctx;c->status=geo_prepare_finish(&c->adapter->engine,&c->ticket,c->status,c->token);return TRUE;
}
static NTSTATUS GeoRenderDrawImpl(struct geo_kmd_context *c,DXGKARG_RENDER *r) {
 struct geo_draw_packet *packet=NULL;struct prepare_call call;struct geo_allocation *a,*v;
 struct geo_dma_ticket t;NTSTATUS status;UINT i;
 if(!c||!c->adapter||!c->adapter->notifications_ready||!c->adapter->synchronize||!r||KeGetCurrentIrql()!=PASSIVE_LEVEL||
    !r->pCommand||r->DmaBufferSegmentId||r->MultipassOffset||r->AllocationListSize!=2u||!r->pAllocationList||
    r->PatchLocationListInSize||!r->pPatchLocationListOut||r->PatchLocationListOutSize<2u||
    !r->pDmaBuffer||((ULONG_PTR)r->pDmaBuffer&3u)||r->DmaSize<sizeof(t)||
    !r->pDmaBufferPrivateData||((ULONG_PTR)r->pDmaBufferPrivateData&3u)||r->DmaBufferPrivateDataSize<sizeof(t)||
    !c->adapter->draw.prepare_draw)return STATUS_INVALID_PARAMETER;
 status=GeoCaptureDrawPacket(&packet,r->pCommand,r->CommandLength);if(status!=STATUS_SUCCESS)return status;
 if(packet->magic!=GEO_DRAW_MAGIC||packet->size!=r->CommandLength||packet->version!=GEO_DRAW_ABI_VERSION||
    packet->reserved||packet->target_index||packet->vertex_index!=1u||packet->request.target||packet->request.vertex||
    packet->request.blob_bytes!=r->CommandLength-sizeof(*packet)||
    !r->pAllocationList[0].WriteOperation||r->pAllocationList[1].WriteOperation){status=STATUS_INVALID_PARAMETER;goto done;}
 a=GeoResolveWriteAllocation(c,r->pAllocationList[0].hDeviceSpecificAllocation);
 v=GeoResolveReadAllocation(c,r->pAllocationList[1].hDeviceSpecificAllocation);
 if(!a||!v||a==v||v->desc.usage!=GEO_USAGE_VERTEX_BUFFER||
    RtlCompareMemory(&a->desc,&packet->request.target_desc,sizeof(a->desc))!=sizeof(a->desc)){status=STATUS_INVALID_PARAMETER;goto done;}
 packet->request.target=a->surface;packet->request.vertex=v->surface;
 status=geo_draw_validate_capture(&packet->request,packet+1,packet->request.blob_bytes);if(status!=STATUS_SUCCESS)goto done;
 if((uint64_t)packet->request.state.vertex_offset+((uint64_t)packet->request.state.first_vertex+packet->request.state.vertex_count)*16u>
    (uint64_t)v->desc.pitch*v->desc.height){status=STATUS_INVALID_PARAMETER;goto done;}
 RtlZeroMemory(&call,sizeof(call));call.adapter=c->adapter;call.allocation=a;call.vertex=v;call.context=c;
 call.open_handle=r->pAllocationList[0].hDeviceSpecificAllocation;call.vertex_open_handle=r->pAllocationList[1].hDeviceSpecificAllocation;
 if(!c->adapter->synchronize(c->adapter->sync_context,begin_prepare,&call)){status=STATUS_DEVICE_NOT_READY;goto done;}
 if(call.status!=STATUS_SUCCESS){status=call.status;goto done;}
 call.status=c->adapter->draw.prepare_draw(c->adapter->draw.context,&packet->request,packet+1,packet->request.blob_bytes,&call.token);
 if(!c->adapter->synchronize(c->adapter->sync_context,finish_prepare,&call)){status=STATUS_DEVICE_NOT_READY;goto done;}
 status=call.status;if(status!=STATUS_SUCCESS)goto done;t=call.ticket;
 if(GeoWriteTicket(r->pDmaBuffer,&t)!=STATUS_SUCCESS||GeoWriteTicket(r->pDmaBufferPrivateData,&t)!=STATUS_SUCCESS){status=STATUS_INVALID_PARAMETER;goto done;}
 r->pDmaBuffer=(UCHAR *)r->pDmaBuffer+sizeof(t);r->pDmaBufferPrivateData=(UCHAR *)r->pDmaBufferPrivateData+sizeof(t);
 for(i=0;i<2u;i++){
  RtlZeroMemory(r->pPatchLocationListOut,sizeof(*r->pPatchLocationListOut));
  r->pPatchLocationListOut->AllocationIndex=i;r->pPatchLocationListOut->DriverId=GEO_DRAW_MAGIC;r->pPatchLocationListOut++;
 }
 done:GeoReleaseDrawCapture(packet);return status;
}
static NTSTATUS GeoRenderImpl(HANDLE context,DXGKARG_RENDER *r) {
 struct geo_kmd_context *c=context;struct prepare_call call;struct geo_clear_packet p;struct geo_dma_ticket t;
 struct geo_allocation *a;NTSTATUS capture_status;
 if(r&&r->CommandLength>=sizeof(struct geo_draw_packet))return GeoRenderDrawImpl(c,r);
 if(!c||!c->adapter||!c->adapter->notifications_ready||!c->adapter->synchronize||!r||KeGetCurrentIrql()!=PASSIVE_LEVEL||!r->pCommand||r->CommandLength!=sizeof(p)||
    r->DmaBufferSegmentId||r->MultipassOffset||r->AllocationListSize!=1||!r->pAllocationList||r->PatchLocationListInSize||!r->pPatchLocationListOut||r->PatchLocationListOutSize<1||
    !r->pDmaBuffer||((ULONG_PTR)r->pDmaBuffer&3)||r->DmaSize<sizeof(t)||!r->pDmaBufferPrivateData||((ULONG_PTR)r->pDmaBufferPrivateData&3)||r->DmaBufferPrivateDataSize<sizeof(t))return STATUS_INVALID_PARAMETER;
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
 if(GeoWriteTicket(r->pDmaBuffer,&t)!=STATUS_SUCCESS||GeoWriteTicket(r->pDmaBufferPrivateData,&t)!=STATUS_SUCCESS)return STATUS_INVALID_PARAMETER;
 r->pDmaBuffer=(UCHAR *)r->pDmaBuffer+sizeof(t);
 r->pDmaBufferPrivateData=(UCHAR *)r->pDmaBufferPrivateData+sizeof(t);
 /* Semantic residency reference. No physical address is stored in a ticket,
  * but VidMm must retain/track the actual referenced allocation. */
 RtlZeroMemory(r->pPatchLocationListOut,sizeof(*r->pPatchLocationListOut));
 r->pPatchLocationListOut->AllocationIndex=0;r->pPatchLocationListOut->DriverId=GEO_CLEAR_MAGIC;
 r->pPatchLocationListOut++;return STATUS_SUCCESS;
}
struct inspect_ticket { struct geo_kmd_adapter *adapter;struct geo_dma_ticket ticket;HANDLE context,open_handle,vertex_open_handle;UINT kind;NTSTATUS status; };
static BOOLEAN inspect_prepared(void *arg) {
 struct inspect_ticket *c=arg;struct geo_job *j;
 c->status=STATUS_INVALID_PARAMETER;
 if(c->ticket.magic!=GEO_TICKET_MAGIC||c->ticket.reserved||c->ticket.slot>=GEO_MAX_JOBS)return TRUE;
 j=&c->adapter->engine.jobs[c->ticket.slot];
 if(!c->adapter->engine.poisoned&&j->state==1&&j->generation==c->ticket.generation&&
    j->context==c->context&&(!c->kind||j->kind==c->kind)&&(!c->open_handle||j->open_handle==c->open_handle)&&
    (!c->vertex_open_handle||j->vertex_open_handle==c->vertex_open_handle))c->status=STATUS_SUCCESS;
 return TRUE;
}
static NTSTATUS GeoPatchImpl(HANDLE adapter,const DXGKARG_PATCH *p) {
 struct geo_dma_ticket t,private_ticket;struct geo_kmd_adapter *a=adapter;struct inspect_ticket inspect;
 const D3DDDI_PATCHLOCATIONLIST *location;UINT count,i,kind;
 if(!p||!p->hContext||!a||!a->synchronize||p->EngineOrdinal||p->DmaBufferSegmentId||p->Flags.Value||
    !p->pPatchLocationList||!p->pAllocationList||!p->pDmaBuffer||!p->pDmaBufferPrivateData||
    p->PatchLocationListSubmissionStart>p->PatchLocationListSize||
    p->PatchLocationListSubmissionLength>p->PatchLocationListSize-p->PatchLocationListSubmissionStart||
    !ticket_range(p->DmaBufferSize,p->DmaBufferSubmissionStartOffset,p->DmaBufferSubmissionEndOffset)||
    !ticket_range(p->DmaBufferPrivateDataSize,p->DmaBufferPrivateDataSubmissionStartOffset,p->DmaBufferPrivateDataSubmissionEndOffset))return STATUS_NOT_SUPPORTED;
 count=p->PatchLocationListSubmissionLength;
 if((count!=1u&&count!=2u)||p->AllocationListSize!=count)return STATUS_INVALID_PARAMETER;
 kind=count==2u?GEO_DRAW_MAGIC:GEO_CLEAR_MAGIC;
 for(i=0;i<count;i++) {
  location=&p->pPatchLocationList[p->PatchLocationListSubmissionStart+i];
  if(location->AllocationIndex!=i||location->Value||location->DriverId!=kind||location->AllocationOffset||location->PatchOffset||location->SplitOffset||
     !p->pAllocationList[i].hDeviceSpecificAllocation||((p->pAllocationList[i].WriteOperation!=0)!=(i==0u)))return STATUS_INVALID_PARAMETER;
 }
 if(GeoReadTicket((UCHAR *)p->pDmaBuffer+p->DmaBufferSubmissionStartOffset,&t)!=STATUS_SUCCESS||
    GeoReadTicket((UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,&private_ticket)!=STATUS_SUCCESS)return STATUS_INVALID_PARAMETER;
 if(t.magic!=GEO_TICKET_MAGIC||t.reserved||t.slot>=GEO_MAX_JOBS||
    t.magic!=private_ticket.magic||t.slot!=private_ticket.slot||t.generation!=private_ticket.generation||t.reserved!=private_ticket.reserved)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&inspect,sizeof(inspect));inspect.adapter=a;inspect.ticket=t;inspect.context=p->hContext;inspect.kind=kind;
 inspect.open_handle=p->pAllocationList[0].hDeviceSpecificAllocation;
 if(count==2u)inspect.vertex_open_handle=p->pAllocationList[1].hDeviceSpecificAllocation;
 if(!a->synchronize(a->sync_context,inspect_prepared,&inspect))return STATUS_DEVICE_NOT_READY;
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
 if(GeoReadTicket((UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,&t)!=STATUS_SUCCESS)return STATUS_INVALID_PARAMETER;
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
 a->queue_dpc(a->dxgk_device);GeoNotifyReadyPreemption(a);return STATUS_SUCCESS;
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
