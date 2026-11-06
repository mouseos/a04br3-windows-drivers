/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * NDIS work items execute at PASSIVE_LEVEL and may free their own handle:
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/nf-ndis-ndisqueueioworkitem
 */
#include <ntddk.h>
#include "native_oid_async.h"
static NDIS_IO_WORKITEM_FUNCTION oid_worker;
static VOID oid_worker(PVOID context,NDIS_HANDLE work)
{
 struct a04_native_oid_async *q=context;PNDIS_OID_REQUEST request;NDIS_HANDLE adapter;NDIS_STATUS status;unsigned cancelled;
 NdisAcquireSpinLock(&q->lock);request=q->request;adapter=q->adapter;cancelled=q->cancelled;q->started=1;NdisReleaseSpinLock(&q->lock);
 status=cancelled?NDIS_STATUS_REQUEST_ABORTED:a04_native_oid_dispatch(q->oid,request);
 NdisFreeIoWorkItem(work);
 /* Completion can reenter the miniport. Detach the request first, but keep
  * rundown active until NDIS has returned from the completion callback. */
 NdisAcquireSpinLock(&q->lock);q->request=NULL;q->busy=0;q->started=0;q->cancelled=0;q->completing++;NdisReleaseSpinLock(&q->lock);
 NdisMOidRequestComplete(adapter,request,status);
 NdisAcquireSpinLock(&q->lock);q->completing--;if(!q->busy&&!q->completing)NdisSetEvent(&q->idle);NdisReleaseSpinLock(&q->lock);
}
NDIS_STATUS a04_native_oid_async_init(struct a04_native_oid_async *q,NDIS_HANDLE adapter,struct a04_native_oid_context *oid)
{
 if(!q||!adapter||!oid||!oid->bridge)return NDIS_STATUS_INVALID_DATA;
 NdisZeroMemory(q,sizeof(*q));q->adapter=adapter;q->oid=oid;NdisAllocateSpinLock(&q->lock);NdisInitializeEvent(&q->idle);NdisSetEvent(&q->idle);q->initialized=1;
 return NDIS_STATUS_SUCCESS;
}
NDIS_STATUS a04_native_oid_async_submit(struct a04_native_oid_async *q,PNDIS_OID_REQUEST request)
{
 NDIS_HANDLE work;NDIS_STATUS status;
 if(!q||!q->initialized||!request)return NDIS_STATUS_INVALID_DATA;
 if(request->RequestType!=NdisRequestSetInformation || request->DATA.SET_INFORMATION.Oid!=OID_DOT11_SCAN_REQUEST)return NDIS_STATUS_NOT_SUPPORTED;
 work=NdisAllocateIoWorkItem(q->adapter);if(!work)return NDIS_STATUS_RESOURCES;
 NdisAcquireSpinLock(&q->lock);
 if(q->closing)status=NDIS_STATUS_ADAPTER_NOT_READY;
 else if(q->busy)status=NDIS_STATUS_DOT11_MEDIA_IN_USE;
 else{q->request=request;q->busy=1;q->started=q->cancelled=0;NdisResetEvent(&q->idle);
  NdisQueueIoWorkItem(work,oid_worker,q);status=NDIS_STATUS_PENDING;}
 NdisReleaseSpinLock(&q->lock);
 if(status!=NDIS_STATUS_PENDING)NdisFreeIoWorkItem(work);
 return status;
}
void a04_native_oid_async_cancel(struct a04_native_oid_async *q,PVOID id)
{
 if(!q||!q->initialized)return;
 NdisAcquireSpinLock(&q->lock);
 if(q->busy && !q->started && q->request && q->request->RequestId==id)q->cancelled=1;
 NdisReleaseSpinLock(&q->lock);
}
NDIS_STATUS a04_native_oid_async_close(struct a04_native_oid_async *q,unsigned timeout_ms)
{
 if(!q||!q->initialized||!timeout_ms||KeGetCurrentIrql()!=PASSIVE_LEVEL)return NDIS_STATUS_INVALID_DATA;
 NdisAcquireSpinLock(&q->lock);q->closing=1;if(q->busy&&!q->started)q->cancelled=1;NdisReleaseSpinLock(&q->lock);
 if(!NdisWaitEvent(&q->idle,timeout_ms))return NDIS_STATUS_PENDING;
 /* Waiter reacquires the lock after the worker signalled under that lock. */
 NdisAcquireSpinLock(&q->lock);if(q->busy||q->completing){NdisReleaseSpinLock(&q->lock);return NDIS_STATUS_PENDING;}
 q->initialized=0;NdisReleaseSpinLock(&q->lock);NdisFreeSpinLock(&q->lock);return NDIS_STATUS_SUCCESS;
}
