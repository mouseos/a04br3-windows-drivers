/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "lifecycle.h"
#include "ticket_io.h"
void GeoNotifyReadyPreemption(struct geo_kmd_adapter *a) {
 DXGKARGCB_NOTIFY_INTERRUPT_DATA data;uint32_t fence,last;
 /* Caller holds the actual interrupt synchronization lock. For idle-only
  * preemption Microsoft explicitly permits NotifyInterrupt from this lock;
  * otherwise this is invoked by the real completed-DMA ISR, never a timer. */
 if(!a||!a->notifications_ready||KeGetCurrentIrql()!=a->owner_irql)return;
 if(geo_take_preemption(&a->engine,&fence,&last)!=GEO_OK)return;
 RtlZeroMemory(&data,sizeof(data));data.InterruptType=DXGK_INTERRUPT_DMA_PREEMPTED;
 data.DmaPreempted.PreemptionFenceId=fence;data.DmaPreempted.LastCompletedFenceId=last;
 data.DmaPreempted.NodeOrdinal=0;data.DmaPreempted.EngineOrdinal=0;
 a->notify_interrupt(a->dxgk_device,&data);InterlockedExchange(&a->dpc_pending,1);a->queue_dpc(a->dxgk_device);
}
struct preempt_call {struct geo_kmd_adapter *adapter;uint32_t fence;NTSTATUS status;};
static BOOLEAN request_preempt(void *arg) {
 struct preempt_call *c=arg;c->status=geo_request_preempt(&c->adapter->engine,c->fence);
 if(c->status==STATUS_SUCCESS)GeoNotifyReadyPreemption(c->adapter);return TRUE;
}
NTSTATUS APIENTRY GeoPreemptCommand(HANDLE handle,const DXGKARG_PREEMPTCOMMAND *p) {
 struct geo_kmd_adapter *a=handle;struct preempt_call call;NTSTATUS status;
 if(!p||p->NodeOrdinal||p->EngineOrdinal||p->Flags.Value)return STATUS_INVALID_PARAMETER;
 if(!GeoSessionEnter(a,3))return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;call.fence=p->PreemptionFenceId;
 status=a->synchronize(a->sync_context,request_preempt,&call)?call.status:STATUS_DEVICE_NOT_READY;
 GeoSessionLeave(a,3);return status; /* Accepted request is not a completion. */
}
struct cancel_call {struct geo_kmd_adapter *adapter;struct geo_dma_ticket ticket;HANDLE context;uint64_t token;NTSTATUS status;};
static BOOLEAN begin_cancel(void *arg) {
 struct cancel_call *c=arg;
 if(c->ticket.magic!=GEO_TICKET_MAGIC||c->ticket.reserved||c->ticket.slot>=GEO_MAX_JOBS){c->status=STATUS_INVALID_PARAMETER;return TRUE;}
 if(c->ticket.generation&&c->ticket.generation<=c->adapter->engine.jobs[c->ticket.slot].retired_generation){
  /* A real completion/cancel/drain already retired this exact generation.
   * A late scheduler cancel is idempotent, even after slot reuse. */
  c->status=STATUS_SUCCESS;c->token=0;return TRUE;
 }
 if(c->adapter->engine.jobs[c->ticket.slot].context!=c->context){c->status=STATUS_INVALID_PARAMETER;return TRUE;}
 c->status=geo_cancel_begin(&c->adapter->engine,&c->ticket,&c->token);return TRUE;
}
static BOOLEAN finish_cancel(void *arg) {
 struct cancel_call *c=arg;c->status=geo_cancel_finish(&c->adapter->engine,&c->ticket,c->status);return TRUE;
}
NTSTATUS APIENTRY GeoCancelCommand(HANDLE handle,const DXGKARG_CANCELCOMMAND *p) {
 struct geo_kmd_adapter *a=handle;struct cancel_call call;NTSTATUS status;
 if(!p||!p->hContext||!p->pDmaBufferPrivateData||p->DmaBufferPrivateDataSubmissionStartOffset>p->DmaBufferPrivateDataSize||
    p->DmaBufferPrivateDataSubmissionEndOffset<p->DmaBufferPrivateDataSubmissionStartOffset||
    p->DmaBufferPrivateDataSubmissionEndOffset>p->DmaBufferPrivateDataSize||
    p->DmaBufferPrivateDataSubmissionEndOffset-p->DmaBufferPrivateDataSubmissionStartOffset!=sizeof(call.ticket))return STATUS_INVALID_PARAMETER;
 if(!GeoSessionEnter(a,2))return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&call,sizeof(call));call.adapter=a;call.context=p->hContext;
 status=GeoReadTicket((UCHAR *)p->pDmaBufferPrivateData+p->DmaBufferPrivateDataSubmissionStartOffset,&call.ticket);
 if(status!=STATUS_SUCCESS)goto done;
 if(!a->synchronize(a->sync_context,begin_cancel,&call)){status=STATUS_DEVICE_NOT_READY;goto done;}
 status=call.status;if(status!=STATUS_SUCCESS||!call.token)goto done;
 /* Job is reserved as never-published. PASSIVE discard can free its prepared
  * resources; submit/ISR cannot race it into hardware during this call. */
 call.status=a->engine.owner.discard_prepared(a->engine.owner.context,call.token);
 if(!a->synchronize(a->sync_context,finish_cancel,&call)){status=STATUS_DEVICE_NOT_READY;goto done;}
 status=call.status;
 done:GeoCollectPendingPayloads(a);GeoSessionLeave(a,2);return status;
}
