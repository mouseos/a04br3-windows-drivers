/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "kmd_clear.h"
#include "capture.h"
#include "lifecycle.h"
#include "kmd_objects.h"
#include "ticket_io.h"
#include "../vendor_m1_11/native_factory_checkpoint.h"
#include "../vendor_m1_11/draw_trace_candidate.h"
static int ticket_range(UINT size,UINT begin,UINT end) {
 return begin<=size&&end>=begin&&end-begin==sizeof(struct geo_dma_ticket)&&end<=size;
}
static NTSTATUS discard_allocation_prepares(struct geo_kmd_adapter *,struct geo_allocation *);
static NTSTATUS prepare_pending(struct geo_kmd_adapter *,const struct geo_dma_ticket *);
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
 /* A CPU-only allocation supports no pitch-aligned GPU segment.
  * WDK8.1 DXGK_ALLOCATIONINFO.PitchAlignedSize is zero in that case. */
 info->PitchAlignedSize=allocation->cpu_only?0:info->Size;info->PreferredSegment.Value=0;info->PreferredSegment.SegmentId0=1;
 info->SupportedReadSegmentSet=1;info->SupportedWriteSegmentSet=1;
 info->EvictionSegmentSet=0;
 info->HintedBank.Value=0;info->MaximumRenamingListLength=0;info->pAllocationUsageHint=NULL;
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
 struct geo_kmd_adapter *a=adapter;struct geo_allocation *allocation,*scan;uint32_t bytes;uint64_t begin,extent,other;NTSTATUS status;PMDL selected_view=NULL;
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(p->Operation==DXGK_OPERATION_MAP_APERTURE_SEGMENT) {
  allocation=p->MapApertureSegment.hAllocation;
  GeoFactoryCheckpoint(24,STATUS_SUCCESS,p->MapApertureSegment.Flags.Value,p->MapApertureSegment.MdlOffset,p->MapApertureSegment.NumberOfPages,allocation!=NULL);
  GeoFactoryCheckpoint(25,STATUS_SUCCESS,p->MapApertureSegment.SegmentId,p->MapApertureSegment.OffsetInPages,
    p->MapApertureSegment.pMdl?MmGetMdlByteCount(p->MapApertureSegment.pMdl):0,
    p->MapApertureSegment.pMdl?MmGetMdlByteOffset(p->MapApertureSegment.pMdl):0);
  /* CacheCoherent requests a coherency guarantee, not hardware snooping.
   * This allocation path always leases the actual cached VidMm PFNs. The
   * native owner flushes for GPU before publish, then CPU only after real
   * GPU/SLC completion, before advertising CPU_VISIBLE/IRQ completion.
   * Reserved bits remain invalid; no segment hardware-coherent cap is set. */
  if(!allocation||p->MapApertureSegment.SegmentId!=1||(p->MapApertureSegment.Flags.Value&~1u)||
     !p->MapApertureSegment.pMdl)return STATUS_NOT_SUPPORTED;
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
  status=GeoBeginVidMmMapSpan(a,allocation,p->MapApertureSegment.pMdl,p->MapApertureSegment.MdlOffset,
    p->MapApertureSegment.NumberOfPages,bytes,&selected_view);
  GeoFactoryCheckpoint(26,status,p->MapApertureSegment.MdlOffset,p->MapApertureSegment.NumberOfPages,bytes,selected_view!=NULL);
  if(status!=STATUS_SUCCESS)return status;
  /* The view identifies this exact PFN slice. Its page offset is already
   * applied by the independently locked span; native receives offset ZERO. */
  status=geo_bind(&a->engine,allocation,selected_view,0,bytes,(uint64_t)p->MapApertureSegment.OffsetInPages*PAGE_SIZE);
  GeoFactoryCheckpoint(27,status,p->MapApertureSegment.OffsetInPages,p->MapApertureSegment.NumberOfPages,bytes,0);
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
  status=discard_allocation_prepares(a,allocation);
  if(status==STATUS_SUCCESS)status=geo_unbind(&a->engine,allocation);
  if(status==STATUS_SUCCESS)GeoEndVidMmMap(a,allocation);return status;
 }
 /* Fill/transfer/discard are not silently accepted without implementing them. */
 return STATUS_NOT_SUPPORTED;
}
struct prepare_call { struct geo_kmd_adapter *adapter;struct geo_allocation *allocation,*vertex,*texture;struct geo_clear_packet packet;struct geo_kmd_context *context;HANDLE open_handle,vertex_open_handle,texture_open_handle;struct geo_dma_ticket ticket;uint64_t token;NTSTATUS status;UINT trace_submit_eligible; };
static BOOLEAN begin_prepare(void *ctx) {
 struct prepare_call *c=ctx;c->status=c->texture?geo_prepare_texture_begin(&c->adapter->engine,c->allocation,c->vertex,c->texture,&c->ticket):c->vertex?geo_prepare_draw_begin(&c->adapter->engine,c->allocation,c->vertex,&c->ticket):geo_prepare_begin(&c->adapter->engine,c->allocation,&c->packet,&c->ticket);
 if(c->status==STATUS_SUCCESS){c->adapter->engine.jobs[c->ticket.slot].context=c->context;c->adapter->engine.jobs[c->ticket.slot].open_handle=c->open_handle;c->adapter->engine.jobs[c->ticket.slot].vertex_open_handle=c->vertex_open_handle;c->adapter->engine.jobs[c->ticket.slot].texture_open_handle=c->texture_open_handle;}return TRUE;
}
static BOOLEAN finish_prepare(void *ctx) {
 struct prepare_call *c=ctx;struct geo_engine *e=&c->adapter->engine;struct geo_job *j=NULL;
 /* Read-only mirror of clear_engine.c:job()/finish predicates under the same
  * IRQ lock; no registry call here. Keep in sync when that function changes. */
 if(c->ticket.magic==GEO_TICKET_MAGIC&&!c->ticket.reserved&&c->ticket.slot<GEO_MAX_JOBS)j=&e->jobs[c->ticket.slot];
 c->trace_submit_eligible=j&&j->generation==c->ticket.generation&&j->state==8&&
  (!j->owner_job||j->owner_job==c->token)&&c->status==GEO_OK&&c->token&&!e->poisoned;
 c->status=geo_finish_submitted_prepare(e,&c->ticket,c->status,c->token);return TRUE;
}
struct payload_collect {struct geo_kmd_adapter *a;struct geo_draw_packet *packets[GEO_MAX_JOBS];};
static BOOLEAN collect_payloads_locked(void *arg) {
 struct payload_collect *c=arg;UINT i;
 for(i=0;i<GEO_MAX_JOBS;i++)if(!c->a->engine.jobs[i].state) {
  c->packets[i]=c->a->pending[i].draw;RtlZeroMemory(&c->a->pending[i],sizeof(c->a->pending[i]));
 }
 return TRUE;
}
void GeoCollectPendingPayloads(struct geo_kmd_adapter *a) {
 struct payload_collect c;UINT i;
 if(!a||!a->synchronize||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 RtlZeroMemory(&c,sizeof(c));c.a=a;
 if(!a->synchronize(a->sync_context,collect_payloads_locked,&c))return;
 for(i=0;i<GEO_MAX_JOBS;i++)GeoReleaseDrawCapture(c.packets[i]);
}
struct pending_change {struct geo_kmd_adapter *a;struct geo_dma_ticket ticket;NTSTATUS status;uint64_t token;UINT restore;};
static BOOLEAN pending_change_locked(void *arg) {
 struct pending_change *c=arg;
 c->status=c->restore?geo_reprepare_finish(&c->a->engine,&c->ticket,c->status):geo_cancel_begin(&c->a->engine,&c->ticket,&c->token);return TRUE;
}
static NTSTATUS discard_allocation_prepares(struct geo_kmd_adapter *a,struct geo_allocation *allocation) {
 UINT i;struct geo_job *j;struct pending_change c;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 for(i=0;i<GEO_MAX_JOBS;i++) {
  j=&a->engine.jobs[i];
  if(!j->state||(j->allocation!=allocation&&j->vertex!=allocation&&j->texture!=allocation))continue;
  if(j->state==4)continue;
  RtlZeroMemory(&c,sizeof(c));c.a=a;c.ticket.magic=GEO_TICKET_MAGIC;c.ticket.slot=i;c.ticket.generation=j->generation;
  if(!a->synchronize(a->sync_context,pending_change_locked,&c))return STATUS_DEVICE_NOT_READY;
  if(c.status!=STATUS_SUCCESS)return c.status; /* Queued/submitted/faulted packets cannot lose residency. */
  c.status=a->engine.owner.discard_prepared(a->engine.owner.context,c.token);c.restore=1;
  if(!a->synchronize(a->sync_context,pending_change_locked,&c))return STATUS_DEVICE_NOT_READY;
  if(c.status!=STATUS_SUCCESS)return c.status;
 }
 return STATUS_SUCCESS;
}
static NTSTATUS prepare_pending(struct geo_kmd_adapter *a,const struct geo_dma_ticket *t) {
 struct geo_job *j;struct geo_pending_payload *payload;struct geo_draw_packet *p;struct geo_draw_packet_v2 *v2;
 struct prepare_call call;UINT header,kind,attempt=0;void *blob;
 ULONGLONG began,service_started,service_elapsed;LARGE_INTEGER interval;NTSTATUS service=STATUS_SUCCESS;
 UINT trace_waited=0,trace_attempt=0,trace_service_ms=0,trace_sync_failed=0;
 if(!a||!t||t->slot>=GEO_MAX_JOBS||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 j=&a->engine.jobs[t->slot];payload=&a->pending[t->slot];
 if(j->generation!=t->generation||payload->generation!=t->generation)return STATUS_INVALID_PARAMETER;
 kind=j->kind;
 if(j->state!=8||!j->allocation||!j->allocation->bound||(j->vertex&&!j->vertex->bound)||(j->texture&&!j->texture->bound))return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;call.ticket=*t;
 if(j->kind==GEO_COPY_MAGIC)call.status=a->engine.owner.prepare_copy?a->engine.owner.prepare_copy(a->engine.owner.context,j->vertex->surface,j->allocation->surface,&call.token):STATUS_NOT_SUPPORTED;
 else if(j->kind==GEO_CLEAR_MAGIC)call.status=a->engine.owner.prepare(a->engine.owner.context,j->allocation->surface,payload->clear.color,&call.token);
 else if(j->kind==GEO_DRAW_MAGIC&&(p=payload->draw)!=NULL) {
  header=p->version==5?sizeof(struct geo_draw_packet_v5):p->version==4?sizeof(struct geo_draw_packet_v4):p->version==3?sizeof(struct geo_draw_packet_v3):p->version==2?sizeof(struct geo_draw_packet_v2):sizeof(*p);blob=(UCHAR *)p+header;v2=(struct geo_draw_packet_v2 *)p;
  GeoDrawTraceSnapshot(&p->request,blob,p->request.blob_bytes,t->slot,t->generation,j->fence);
  call.status=p->version==5?a->draw_v5.prepare_draw(a->draw_v5.context,&((struct geo_draw_packet_v5 *)p)->request,blob,p->request.blob_bytes,&call.token):p->version==4?a->draw_v4.prepare_draw(a->draw_v4.context,&((struct geo_draw_packet_v4 *)p)->request,blob,p->request.blob_bytes,&call.token):p->version==3?a->draw_v3.prepare_draw(a->draw_v3.context,&((struct geo_draw_packet_v3 *)p)->request,blob,p->request.blob_bytes,&call.token):p->version==2?a->draw_v2.prepare_draw(a->draw_v2.context,&v2->request,blob,p->request.blob_bytes,&call.token):
    a->draw.prepare_draw(a->draw.context,&p->request,blob,p->request.blob_bytes,&call.token);
 }else return STATUS_INVALID_PARAMETER;
 /* Persist entry before starting the bounded real-IRQ continuation budget. */
 if(kind==GEO_DRAW_MAGIC)GeoFactoryCheckpoint(55,STATUS_PENDING,t->slot,t->generation,0,8);
 began=KeQueryInterruptTime();interval.QuadPart=-10000;
 for(;;) {
  if(!a->synchronize(a->sync_context,finish_prepare,&call)){
   call.status=STATUS_DEVICE_NOT_READY;trace_sync_failed=1;goto record_result;
  }
  if(call.status!=GEO_SUBMIT_WAIT_IRQ)break;
  trace_waited=1;trace_attempt=attempt;
  /* The actual synchronized submit predicate gets the first decision after
   * successful IRQ service. Expiry only forbids another wait/service; it
   * must not replace a completed rearm with an invented submit failure. */
  if(attempt>=100 || KeQueryInterruptTime()-began>=1000000u){call.status=STATUS_IO_TIMEOUT;goto fail_claim;}
  InterlockedAnd(&a->service_pending,~1L); /* consume BEFORE service/rearm */
  service_started=KeQueryInterruptTime();
  if(!a->session || !a->session->control.service)service=STATUS_INVALID_DEVICE_STATE;
  else service=a->session->control.service(a->session->lease.cookie,a->session->lease.epoch);
  service_elapsed=KeQueryInterruptTime()-service_started;
  trace_service_ms=(ULONG)(service_elapsed/10000u>MAXULONG?MAXULONG:service_elapsed/10000u);
  ++attempt;
  if(service!=STATUS_SUCCESS && service!=STATUS_DEVICE_BUSY){call.status=service;goto fail_claim;}
  if(service==STATUS_DEVICE_BUSY){
   if(attempt>=100 || KeQueryInterruptTime()-began>=1000000u){call.status=STATUS_IO_TIMEOUT;goto fail_claim;}
   (void)KeDelayExecutionThread(KernelMode,FALSE,&interval);
  }
  /* No fake completion: this only retries the SAME state8/token through
   * real owner.submit under the real IRQ lock. A renewed IRQ claim must
   * still satisfy the unchanged count/elapsed bound above before service. */
  call.status=STATUS_SUCCESS;
 }
 goto record_result;
fail_claim:
 /* Preserve the existing poisoned/retained-token failure path. */
 if(!a->synchronize(a->sync_context,finish_prepare,&call)){
  call.status=STATUS_DEVICE_NOT_READY;trace_sync_failed=1;
 }
record_result:
 /* Registry IO is outside the bounded loop. R50/R51 now describe its last
  * actual claim/service; their FILETIME is persistence time, not call time.
  * R51.d is measured service elapsed milliseconds (record contract353). */
 if(trace_waited)GeoFactoryCheckpoint(50,GEO_SUBMIT_WAIT_IRQ,kind,t->slot,t->generation,trace_attempt);
 if(attempt)GeoFactoryCheckpoint(51,service,kind,t->slot,t->generation,trace_service_ms);
 if(kind==GEO_DRAW_MAGIC)GeoFactoryCheckpoint(55,call.status,t->slot,t->generation,attempt,trace_sync_failed?2:4|(call.trace_submit_eligible?1:0));
 GeoFactoryCheckpoint(23,call.status,kind,t->slot,t->generation,0);return call.status;
}
struct claim_prepare_call {struct geo_kmd_adapter *adapter;struct geo_dma_ticket ticket;NTSTATUS status;};
static BOOLEAN claim_prepare_locked(void *arg) {
 struct claim_prepare_call *c=arg;c->status=geo_claim_submitted_prepare(&c->adapter->engine,&c->ticket);return TRUE;
}
NTSTATUS GeoPrepareSubmittedWork(struct geo_kmd_adapter *a) {
 struct claim_prepare_call c;struct geo_job *j;NTSTATUS status;ULONG kind,fence;
 if(!a||!a->synchronize||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 RtlZeroMemory(&c,sizeof(c));c.adapter=a;
 if(!a->synchronize(a->sync_context,claim_prepare_locked,&c))return STATUS_DEVICE_NOT_READY;
 if(c.status==GEO_NOT_READY)return STATUS_SUCCESS; /* No runnable head, no hardware request. */
 if(c.status!=STATUS_SUCCESS)return c.status;
 j=&a->engine.jobs[c.ticket.slot];
 kind=j->kind;fence=j->fence;
 GeoFactoryCheckpoint(48,STATUS_SUCCESS,kind,c.ticket.slot,c.ticket.generation,fence);
 status=prepare_pending(a,&c.ticket);
 GeoFactoryCheckpoint(49,status,kind,c.ticket.slot,c.ticket.generation,fence);
 return status;
}
static NTSTATUS GeoRenderDrawImpl(struct geo_kmd_context *c,DXGKARG_RENDER *r) {
 struct geo_draw_packet *packet=NULL;struct prepare_call call;struct geo_allocation *a,*v,*x=NULL;
 struct geo_dma_ticket t;NTSTATUS status;UINT i,header,count;void *blob;struct geo_draw_packet_v2 *v2;
 if(!c||!c->adapter||!c->adapter->notifications_ready||!c->adapter->synchronize||!r||KeGetCurrentIrql()!=PASSIVE_LEVEL||
    !r->pCommand||r->DmaBufferSegmentId||r->MultipassOffset||r->AllocationListSize<2u||!r->pAllocationList||
    r->PatchLocationListInSize||!r->pPatchLocationListOut||r->PatchLocationListOutSize<2u||
    !r->pDmaBuffer||((ULONG_PTR)r->pDmaBuffer&3u)||r->DmaSize<sizeof(t)||
    !r->pDmaBufferPrivateData||((ULONG_PTR)r->pDmaBufferPrivateData&3u)||r->DmaBufferPrivateDataSize<sizeof(t)||
    !c->adapter->draw.prepare_draw)return STATUS_INVALID_PARAMETER;
 status=GeoCaptureDrawPacket(&packet,r->pCommand,r->CommandLength);if(status!=STATUS_SUCCESS)return status;
 header=packet->version==5?sizeof(struct geo_draw_packet_v5):packet->version==4?sizeof(struct geo_draw_packet_v4):packet->version==3?sizeof(struct geo_draw_packet_v3):packet->version==2?sizeof(struct geo_draw_packet_v2):sizeof(*packet);
 count=(packet->version==3||packet->version==5)?3:2;
 v2=(struct geo_draw_packet_v2 *)packet;
 if(r->CommandLength<header){status=STATUS_INVALID_PARAMETER;goto done;}
 blob=(UCHAR *)packet+header;
 if((packet->version==5&&!c->adapter->draw_v5.prepare_draw)||(packet->version==4&&!c->adapter->draw_v4.prepare_draw)||(packet->version==3&&!c->adapter->draw_v3.prepare_draw)||(packet->version==2&&!c->adapter->draw_v2.prepare_draw)){status=STATUS_NOT_SUPPORTED;goto done;}
 if(packet->magic!=GEO_DRAW_MAGIC||packet->size!=r->CommandLength||(packet->version!=GEO_DRAW_ABI_VERSION&&packet->version!=2&&packet->version!=3&&packet->version!=4&&packet->version!=5)||
    packet->reserved||packet->target_index||packet->vertex_index!=1u||packet->request.target||packet->request.vertex||
    packet->request.blob_bytes!=r->CommandLength-header||
    !r->pAllocationList[0].WriteOperation||r->pAllocationList[1].WriteOperation){status=STATUS_INVALID_PARAMETER;goto done;}
 if(r->AllocationListSize<count||r->PatchLocationListOutSize<count){status=STATUS_INVALID_PARAMETER;goto done;}
 if(packet->version==3||packet->version==5){struct geo_draw_request_v3 *sampled=packet->version==5?&((struct geo_draw_packet_v5 *)packet)->request.sampled:&((struct geo_draw_packet_v3 *)packet)->request;
  UINT texture_index=packet->version==5?((struct geo_draw_packet_v5 *)packet)->texture_index:((struct geo_draw_packet_v3 *)packet)->texture_index;
  UINT reserved_texture=packet->version==5?((struct geo_draw_packet_v5 *)packet)->reserved_texture:((struct geo_draw_packet_v3 *)packet)->reserved_texture;
  if(texture_index!=2||reserved_texture||sampled->texture||r->pAllocationList[2].WriteOperation){status=STATUS_INVALID_PARAMETER;goto done;}
  x=GeoResolveReadAllocation(c,r->pAllocationList[2].hDeviceSpecificAllocation);
  if(!x||x->desc.version!=GEO_TEXTURE_STORAGE_VERSION||x->desc.usage!=GEO_USAGE_TEXTURE2D||RtlCompareMemory(&x->desc,&sampled->texture_desc,sizeof(x->desc))!=sizeof(x->desc)){status=STATUS_INVALID_PARAMETER;goto done;}
  sampled->texture=x->surface;
 }
 a=GeoResolveWriteAllocation(c,r->pAllocationList[0].hDeviceSpecificAllocation);
 v=GeoResolveReadAllocation(c,r->pAllocationList[1].hDeviceSpecificAllocation);
 if(!a||!v||a==v||(x&&(x==a||x==v))||v->desc.usage!=GEO_USAGE_VERTEX_BUFFER||
    RtlCompareMemory(&a->desc,&packet->request.target_desc,sizeof(a->desc))!=sizeof(a->desc)){status=STATUS_INVALID_PARAMETER;goto done;}
 packet->request.target=a->surface;packet->request.vertex=v->surface;
 status=packet->version==5?geo_draw_validate_capture_v5(&((struct geo_draw_packet_v5 *)packet)->request,blob,packet->request.blob_bytes):packet->version==4?geo_draw_validate_capture_v4(&((struct geo_draw_packet_v4 *)packet)->request,blob,packet->request.blob_bytes):packet->version==3?geo_draw_validate_capture_v3(&((struct geo_draw_packet_v3 *)packet)->request,blob,packet->request.blob_bytes):packet->version==2?geo_draw_validate_capture_v2(&v2->request,blob,packet->request.blob_bytes):geo_draw_validate_capture(&packet->request,blob,packet->request.blob_bytes);if(status!=STATUS_SUCCESS)goto done;
 if((uint64_t)packet->request.state.vertex_offset+((uint64_t)packet->request.state.first_vertex+packet->request.state.vertex_count)*packet->request.state.vertex_stride>
    (uint64_t)v->desc.pitch*v->desc.height){status=STATUS_INVALID_PARAMETER;goto done;}
 RtlZeroMemory(&call,sizeof(call));call.adapter=c->adapter;call.allocation=a;call.vertex=v;call.texture=x;call.context=c;
 call.open_handle=r->pAllocationList[0].hDeviceSpecificAllocation;call.vertex_open_handle=r->pAllocationList[1].hDeviceSpecificAllocation;
 if(x)call.texture_open_handle=r->pAllocationList[2].hDeviceSpecificAllocation;
 if(!c->adapter->synchronize(c->adapter->sync_context,begin_prepare,&call)){status=STATUS_DEVICE_NOT_READY;goto done;}
 if(call.status!=STATUS_SUCCESS){status=call.status;goto done;}
 t=call.ticket;c->adapter->pending[t.slot].generation=t.generation;c->adapter->pending[t.slot].draw=packet;packet=NULL;
 /* Render captures commands; residency does not mean the scheduler is about
  * to submit this packet. Preparing here can reserve the single native job
  * for an unsent Clear and deadlock a following Draw inside Render. Keep
  * the retained capture/ticket pending until validated DxgkDdiPatch. */
 status=STATUS_SUCCESS;
 if(GeoWriteTicket(r->pDmaBuffer,&t)!=STATUS_SUCCESS||GeoWriteTicket(r->pDmaBufferPrivateData,&t)!=STATUS_SUCCESS){status=STATUS_INVALID_PARAMETER;goto done;}
 r->pDmaBuffer=(UCHAR *)r->pDmaBuffer+sizeof(t);
 r->pDmaBufferPrivateData=(UCHAR *)r->pDmaBufferPrivateData+sizeof(t);
 for(i=0;i<count;i++){
  RtlZeroMemory(r->pPatchLocationListOut,sizeof(*r->pPatchLocationListOut));
  r->pPatchLocationListOut->AllocationIndex=i;r->pPatchLocationListOut->DriverId=GEO_DRAW_MAGIC;r->pPatchLocationListOut++;
 }
 done:GeoReleaseDrawCapture(packet);return status;
}
struct copy_begin {struct geo_kmd_context *context;struct geo_allocation *destination,*source;HANDLE dh,sh;struct geo_dma_ticket ticket;NTSTATUS status;};
static BOOLEAN begin_copy(void *arg){struct copy_begin *c=arg;struct geo_job *j;
 c->status=geo_prepare_copy_begin(&c->context->adapter->engine,c->destination,c->source,&c->ticket);
 if(c->status==STATUS_SUCCESS){j=&c->context->adapter->engine.jobs[c->ticket.slot];j->context=c->context;j->open_handle=c->dh;j->vertex_open_handle=c->sh;}return TRUE;}
/* A failed Render has no published ticket for scheduler cancellation.
 * Keep both refs and any possibly prepared native token, close normal work,
 * and retire only through lifecycle apply_proof after real owner drain.
 * Do not free an ambiguous native prepare or pretend it never existed. */
static BOOLEAN poison_copy_render(void *arg) {
 struct geo_kmd_adapter *a=arg;a->engine.poisoned=1;return TRUE;
}
static NTSTATUS fail_copy_render(struct geo_kmd_adapter *a,NTSTATUS status) {
 (void)a->synchronize(a->sync_context,poison_copy_render,a);
 /* Even a failed synchronize must not allow more normal session work. */
 if(a->session)InterlockedExchange(&a->session->state,GEO_SESSION_FAILED);
 return status;
}
static NTSTATUS GeoRenderCopyImpl(struct geo_kmd_context *c,DXGKARG_RENDER *r,const struct geo_copy_packet *p) {
 struct copy_begin x;UINT i;
 if(p->magic!=GEO_COPY_MAGIC||p->size!=sizeof(*p)||p->version!=1||p->destination_index||p->source_index!=1||p->width!=800||p->height!=600||p->reserved||
    r->AllocationListSize<2||r->PatchLocationListOutSize<2||!r->pAllocationList[0].WriteOperation||r->pAllocationList[1].WriteOperation)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&x,sizeof(x));x.context=c;x.dh=r->pAllocationList[0].hDeviceSpecificAllocation;x.sh=r->pAllocationList[1].hDeviceSpecificAllocation;
 x.destination=GeoResolveWriteAllocation(c,x.dh);x.source=GeoResolveReadAllocation(c,x.sh);
 if(!x.destination||!x.source||!c->adapter->engine.owner.prepare_copy)return STATUS_INVALID_PARAMETER;
 if(!c->adapter->synchronize(c->adapter->sync_context,begin_copy,&x))return fail_copy_render(c->adapter,STATUS_DEVICE_NOT_READY);
 if(x.status!=STATUS_SUCCESS)return x.status;
 c->adapter->pending[x.ticket.slot].generation=x.ticket.generation;
 /* Patch supplies the final residency and prepares this queued Copy. */
 if(GeoWriteTicket(r->pDmaBuffer,&x.ticket)!=STATUS_SUCCESS||GeoWriteTicket(r->pDmaBufferPrivateData,&x.ticket)!=STATUS_SUCCESS)return fail_copy_render(c->adapter,STATUS_INVALID_PARAMETER);
 r->pDmaBuffer=(UCHAR*)r->pDmaBuffer+sizeof(x.ticket);r->pDmaBufferPrivateData=(UCHAR*)r->pDmaBufferPrivateData+sizeof(x.ticket);
 for(i=0;i<2;i++){RtlZeroMemory(r->pPatchLocationListOut,sizeof(*r->pPatchLocationListOut));r->pPatchLocationListOut->AllocationIndex=i;r->pPatchLocationListOut->DriverId=GEO_COPY_MAGIC;r->pPatchLocationListOut++;}return STATUS_SUCCESS;
}
static NTSTATUS GeoRenderImpl(HANDLE context,DXGKARG_RENDER *r) {
 struct geo_kmd_context *c=context;struct prepare_call call;struct geo_clear_packet p;struct geo_dma_ticket t;
 struct geo_allocation *a;NTSTATUS capture_status;
 if(r&&r->CommandLength>=sizeof(struct geo_draw_packet))return GeoRenderDrawImpl(c,r);
 if(!c||!c->adapter||!c->adapter->notifications_ready||!c->adapter->synchronize||!r||KeGetCurrentIrql()!=PASSIVE_LEVEL||!r->pCommand||r->CommandLength!=sizeof(p)||
    r->DmaBufferSegmentId||r->MultipassOffset||r->AllocationListSize<1||!r->pAllocationList||r->PatchLocationListInSize||!r->pPatchLocationListOut||r->PatchLocationListOutSize<1||
    !r->pDmaBuffer||((ULONG_PTR)r->pDmaBuffer&3)||r->DmaSize<sizeof(t)||!r->pDmaBufferPrivateData||((ULONG_PTR)r->pDmaBufferPrivateData&3)||r->DmaBufferPrivateDataSize<sizeof(t))return STATUS_INVALID_PARAMETER;
 capture_status=GeoCaptureClearPacket(&p,r->pCommand);
 if(capture_status!=STATUS_SUCCESS)return capture_status;
 if(p.magic==GEO_COPY_MAGIC){struct geo_copy_packet copy;RtlCopyMemory(&copy,&p,sizeof(copy));return GeoRenderCopyImpl(c,r,&copy);}
 if(!geo_packet_valid(&p,1)||!r->pAllocationList[0].WriteOperation)return STATUS_INVALID_PARAMETER;
 a=GeoResolveWriteAllocation(c,r->pAllocationList[0].hDeviceSpecificAllocation);
 /* Full CreateAllocation/DestroyAllocation owns and validates this handle.
  * VidMm provides this kernel list; UMD cannot pass a pointer in the packet. */
 if(!a)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&call,sizeof(call));call.adapter=c->adapter;call.allocation=a;call.packet=p;call.context=c;call.open_handle=r->pAllocationList[0].hDeviceSpecificAllocation;
 if(!c->adapter->synchronize(c->adapter->sync_context,begin_prepare,&call))return STATUS_DEVICE_NOT_READY;
 if(call.status!=STATUS_SUCCESS)return call.status;
 t=call.ticket;c->adapter->pending[t.slot].generation=t.generation;c->adapter->pending[t.slot].clear=p;
 /* Capture only, including resident allocations: Patch owns native prepare. */
 if(GeoWriteTicket(r->pDmaBuffer,&t)!=STATUS_SUCCESS||GeoWriteTicket(r->pDmaBufferPrivateData,&t)!=STATUS_SUCCESS)return STATUS_INVALID_PARAMETER;
 r->pDmaBuffer=(UCHAR *)r->pDmaBuffer+sizeof(t);
 /* RT 8.1 computes private submission length from this returned cursor.
  * Both ticket copies must consume the same bytes; capacity fields stay input. */
 r->pDmaBufferPrivateData=(UCHAR *)r->pDmaBufferPrivateData+sizeof(t);
 /* Semantic residency reference. No physical address is stored in a ticket,
  * but VidMm must retain/track the actual referenced allocation. */
 RtlZeroMemory(r->pPatchLocationListOut,sizeof(*r->pPatchLocationListOut));
 r->pPatchLocationListOut->AllocationIndex=0;r->pPatchLocationListOut->DriverId=GEO_CLEAR_MAGIC;
 r->pPatchLocationListOut++;return STATUS_SUCCESS;
}
struct inspect_ticket { struct geo_kmd_adapter *adapter;struct geo_dma_ticket ticket;HANDLE context,open_handle,vertex_open_handle,texture_open_handle;UINT kind,count;NTSTATUS status;UINT allow_pending; };
static BOOLEAN inspect_prepared(void *arg) {
 struct inspect_ticket *c=arg;struct geo_job *j;
 c->status=STATUS_INVALID_PARAMETER;
 if(c->ticket.magic!=GEO_TICKET_MAGIC||c->ticket.reserved||c->ticket.slot>=GEO_MAX_JOBS)return TRUE;
 j=&c->adapter->engine.jobs[c->ticket.slot];
 if(!c->adapter->engine.poisoned&&(j->state==1||(c->allow_pending&&j->state==4))&&
    (j->state!=1||j->owner_job)&&j->generation==c->ticket.generation&&
    j->allocation&&j->allocation->bound&&j->allocation->surface&&
    (!j->vertex||(j->vertex->bound&&j->vertex->surface))&&(!j->texture||(j->texture->bound&&j->texture->surface))&&
    j->context==c->context&&(!c->kind||j->kind==c->kind)&&(!c->open_handle||j->open_handle==c->open_handle)&&
    (!c->vertex_open_handle||j->vertex_open_handle==c->vertex_open_handle)&&
    (!c->texture_open_handle||j->texture_open_handle==c->texture_open_handle)&&(!c->count||c->count==(j->texture?3u:j->vertex?2u:1u)))c->status=STATUS_SUCCESS;
 return TRUE;
}
/* R18 is reserved for a failing Patch: fixed phase + semantic kind/ticket.
 * GeoFactoryCheckpoint flushes all failing NTSTATUS records at PASSIVE.
 * R19 is reserved for native exact-stage failure (owner implementation).
 * No pointers, MMIO reads, success suppression, or successful-path flush. */
static NTSTATUS patch_failure(NTSTATUS status,UINT phase,UINT kind,const struct geo_dma_ticket *t) {
 GeoFactoryCheckpoint(18,status,phase,kind,t?t->slot:UINT32_MAX,t?t->generation:0);
 return status;
}
static NTSTATUS GeoPatchImpl(HANDLE adapter,const DXGKARG_PATCH *p) {
 struct geo_dma_ticket t,private_ticket;struct geo_kmd_adapter *a=adapter;struct inspect_ticket inspect;
 const D3DDDI_PATCHLOCATIONLIST *location;UINT count=0,i,kind=0,phase=1;NTSTATUS status;
 RtlZeroMemory(&t,sizeof(t));
 if(p) {
  GeoFactoryCheckpoint(28,STATUS_SUCCESS,p->Flags.Value,p->DmaBufferSize,p->DmaBufferSubmissionStartOffset,p->DmaBufferSubmissionEndOffset);
  GeoFactoryCheckpoint(29,STATUS_SUCCESS,p->hContext!=NULL,p->DmaBufferPrivateDataSize,p->DmaBufferPrivateDataSubmissionStartOffset,p->DmaBufferPrivateDataSubmissionEndOffset);
 }
 if(!p||!p->hContext||!a||!a->synchronize||p->EngineOrdinal||p->DmaBufferSegmentId||p->Flags.Value||
    !p->pPatchLocationList||!p->pAllocationList||!p->pDmaBuffer||!p->pDmaBufferPrivateData||
    p->PatchLocationListSubmissionStart>p->PatchLocationListSize||
    p->PatchLocationListSubmissionLength>p->PatchLocationListSize-p->PatchLocationListSubmissionStart||
    !ticket_range(p->DmaBufferSize,p->DmaBufferSubmissionStartOffset,p->DmaBufferSubmissionEndOffset)||
    !ticket_range(p->DmaBufferPrivateDataSize,p->DmaBufferPrivateDataSubmissionStartOffset,p->DmaBufferPrivateDataSubmissionEndOffset))return patch_failure(STATUS_NOT_SUPPORTED,phase,kind,&t);
 phase=2;count=p->PatchLocationListSubmissionLength;
 if((count!=1u&&count!=2u&&count!=3u)||p->AllocationListSize<count)return patch_failure(STATUS_INVALID_PARAMETER,phase,kind,&t);
 kind=p->pPatchLocationList[p->PatchLocationListSubmissionStart].DriverId;
 if((count==1&&kind!=GEO_CLEAR_MAGIC)||(count==2&&kind!=GEO_DRAW_MAGIC&&kind!=GEO_COPY_MAGIC)||(count==3&&kind!=GEO_DRAW_MAGIC))return patch_failure(STATUS_INVALID_PARAMETER,phase,kind,&t);
 phase=3;for(i=0;i<count;i++) {
  location=&p->pPatchLocationList[p->PatchLocationListSubmissionStart+i];
  if(location->AllocationIndex!=i||location->Value||location->DriverId!=kind||location->AllocationOffset||location->PatchOffset||location->SplitOffset||
     !p->pAllocationList[i].hDeviceSpecificAllocation||((p->pAllocationList[i].WriteOperation!=0)!=(i==0u)))return patch_failure(STATUS_INVALID_PARAMETER,phase,kind,&t);
 }
 phase=4;if(GeoReadTicket((UCHAR *)p->pDmaBuffer+p->DmaBufferSubmissionStartOffset,&t)!=STATUS_SUCCESS||
    GeoReadTicket((UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,&private_ticket)!=STATUS_SUCCESS)return patch_failure(STATUS_INVALID_PARAMETER,phase,kind,&t);
 if(t.magic!=GEO_TICKET_MAGIC||t.reserved||t.slot>=GEO_MAX_JOBS||
    t.magic!=private_ticket.magic||t.slot!=private_ticket.slot||t.generation!=private_ticket.generation||t.reserved!=private_ticket.reserved)return patch_failure(STATUS_INVALID_PARAMETER,phase,kind,&t);
 RtlZeroMemory(&inspect,sizeof(inspect));inspect.adapter=a;inspect.ticket=t;inspect.context=p->hContext;inspect.kind=kind;inspect.count=count;
 inspect.open_handle=p->pAllocationList[0].hDeviceSpecificAllocation;
 if(count>=2u)inspect.vertex_open_handle=p->pAllocationList[1].hDeviceSpecificAllocation;
 if(count==3u)inspect.texture_open_handle=p->pAllocationList[2].hDeviceSpecificAllocation;
 /* The OS will bugcheck on any Patch error. A malformed context/list must
  * not first arm real firmware work. Validate both endpoints and residency
  * under the same IRQ lock before touching the native prepare API. */
 phase=5;inspect.allow_pending=1;
 if(!a->synchronize(a->sync_context,inspect_prepared,&inspect))return patch_failure(STATUS_DEVICE_NOT_READY,phase,kind,&t);
 if(inspect.status!=STATUS_SUCCESS)return patch_failure(inspect.status,phase,kind,&t);
 /* Patch can be skipped for resident buffers or repeated before submission.
  * Validate the retained capture/residency only. Submit accepts the fence,
  * then the PASSIVE worker prepares exactly the selected FIFO head. */
 phase=7;inspect.allow_pending=1;
 if(!a->synchronize(a->sync_context,inspect_prepared,&inspect))return patch_failure(STATUS_DEVICE_NOT_READY,phase,kind,&t);
 return inspect.status==STATUS_SUCCESS?STATUS_SUCCESS:patch_failure(inspect.status,phase,kind,&t);
}
struct submit_call { struct geo_kmd_adapter *adapter;struct geo_dma_ticket ticket;uint32_t fence;HANDLE context;NTSTATUS status;UINT queue_prepare; };
static BOOLEAN submit_locked(void *ctx) {
 struct submit_call *c=ctx;struct inspect_ticket inspect;
 RtlZeroMemory(&inspect,sizeof(inspect));inspect.adapter=c->adapter;inspect.ticket=c->ticket;inspect.context=c->context;inspect.allow_pending=1;
 inspect_prepared(&inspect);if(inspect.status!=STATUS_SUCCESS){c->status=inspect.status;return TRUE;}
 c->status=geo_submit_captured(&c->adapter->engine,&c->ticket,c->fence);
 if(c->status==STATUS_SUCCESS)c->queue_prepare=geo_submitted_prepare_ready(&c->adapter->engine);
 return TRUE;
}
static NTSTATUS GeoSubmitCommandImpl(HANDLE adapter,const DXGKARG_SUBMITCOMMAND *p) {
 struct geo_kmd_adapter *a=adapter;struct geo_dma_ticket t;struct submit_call call;
 if(!a||!a->notifications_ready||!a->synchronize||!p||KeGetCurrentIrql()>DISPATCH_LEVEL||p->EngineOrdinal||p->NodeOrdinal||p->DmaBufferSegmentId||
    p->Flags.Value||!p->pDmaBufferPrivateData||
    !ticket_range(p->DmaBufferPrivateDataSize,p->DmaBufferPrivateDataSubmissionStartOffset,p->DmaBufferPrivateDataSubmissionEndOffset))return STATUS_NOT_SUPPORTED;
 if(GeoReadTicket((UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,&t)!=STATUS_SUCCESS)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;call.ticket=t;call.fence=p->SubmissionFenceId;call.context=p->hContext;
 if(!a->synchronize(a->sync_context,submit_locked,&call))return STATUS_DEVICE_NOT_READY;
 if(call.status==STATUS_SUCCESS&&call.queue_prepare){InterlockedOr(&a->service_pending,2);GeoQueueSubmittedPrepare(a);}
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
 if(geo_submitted_prepare_ready(&a->engine))InterlockedOr(&a->service_pending,2);
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
 if(!a||!a->session||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 GeoFactoryCheckpoint(8,STATUS_SUCCESS,(ULONG)p->Operation,KeGetCurrentIrql(),0,0);
 mode=p->Operation==DXGK_OPERATION_UNMAP_APERTURE_SEGMENT?2:1;
 /* RT81 VidMm accepts no busy paging result (verified with the shipped
  * dxgmms1). Native residency must complete the requested mapping; never
  * invent SUCCESS or INSUFFICIENT_DMA_BUFFER to hide an owner failure. */
 if(!GeoSessionEnter(a,mode))return STATUS_DEVICE_NOT_READY;
 status=GeoBuildPagingBufferImpl(a,p);
 GeoFactoryCheckpoint(21,status,(ULONG)p->Operation,0,0,0);GeoSessionLeave(a,mode);return status;
}
NTSTATUS APIENTRY GeoRender(HANDLE handle,DXGKARG_RENDER *p) {
 struct geo_kmd_context *c=handle;NTSTATUS status;
 GeoFactoryCheckpoint(6,STATUS_SUCCESS,p?p->CommandLength:0,p?p->AllocationListSize:0,KeGetCurrentIrql(),0);
 if(!c||!GeoSessionEnter(c->adapter,1))return STATUS_DEVICE_NOT_READY;
 GeoFactoryCheckpoint(7,STATUS_SUCCESS,0,0,0,0);
 GeoCollectPendingPayloads(c->adapter);status=GeoRenderImpl(c,p);GeoFactoryCheckpoint(20,status,p?p->CommandLength:0,p?p->AllocationListSize:0,p&&p->pAllocationList&&p->AllocationListSize?p->pAllocationList[0].SegmentId:0,0);GeoSessionLeave(c->adapter,1);return status;
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
 GeoFactoryCheckpoint(9,STATUS_SUCCESS,p?p->AllocationListSize:0,KeGetCurrentIrql(),0,0);
 if(!GeoSessionEnter(a,1))return patch_failure(STATUS_DEVICE_NOT_READY,0,0,NULL);
 GeoCollectPendingPayloads(a);status=GeoPatchImpl(a,p);GeoFactoryCheckpoint(22,status,p?p->AllocationListSize:0,p?p->PatchLocationListSubmissionLength:0,0,0);GeoSessionLeave(a,1);return status;
}
