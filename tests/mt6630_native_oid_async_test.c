/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "native_oid_async.h"
static NDIS_IO_WORKITEM_FUNCTION *queued;static void *queued_ctx;static int completed,freed,dispatched;static NDIS_STATUS last;
static struct a04_native_oid_async *close_in_completion;
int KeGetCurrentIrql(void){return 0;}
void NdisAllocateSpinLock(NDIS_SPIN_LOCK *l){l->held=0;}void NdisFreeSpinLock(NDIS_SPIN_LOCK *l){assert(!l->held);}
void NdisAcquireSpinLock(NDIS_SPIN_LOCK *l){assert(!l->held);l->held=1;}void NdisReleaseSpinLock(NDIS_SPIN_LOCK *l){assert(l->held);l->held=0;}
void NdisInitializeEvent(NDIS_EVENT *e){e->signaled=0;}void NdisSetEvent(NDIS_EVENT *e){e->signaled=1;}void NdisResetEvent(NDIS_EVENT *e){e->signaled=0;}int NdisWaitEvent(NDIS_EVENT *e,unsigned t){assert(t);return e->signaled;}
NDIS_HANDLE NdisAllocateIoWorkItem(NDIS_HANDLE adapter){assert(adapter);return (void *)1;}void NdisFreeIoWorkItem(NDIS_HANDLE w){assert(w);freed++;}
void NdisQueueIoWorkItem(NDIS_HANDLE w,NDIS_IO_WORKITEM_FUNCTION *f,PVOID c){assert(w && !queued);queued=f;queued_ctx=c;}
void NdisMOidRequestComplete(NDIS_HANDLE a,PNDIS_OID_REQUEST r,NDIS_STATUS s){assert(a && r);completed++;last=s;if(close_in_completion){assert(a04_native_oid_async_close(close_in_completion,1)==NDIS_STATUS_PENDING);close_in_completion=NULL;}}
NDIS_STATUS a04_native_oid_dispatch(struct a04_native_oid_context *c,PNDIS_OID_REQUEST r){assert(c&&r);dispatched++;return NDIS_STATUS_SUCCESS;}
static void run(void){NDIS_IO_WORKITEM_FUNCTION *f=queued;void *c=queued_ctx;assert(f);queued=NULL;queued_ctx=NULL;f(c,(void *)1);}
int main(void)
{
 struct a04_native_oid_async q;struct a04_native_oid_context c={0};NDIS_OID_REQUEST r={0};int id;
 c.bridge=(void *)1;r.RequestType=NdisRequestSetInformation;r.RequestId=&id;r.DATA.SET_INFORMATION.Oid=OID_DOT11_SCAN_REQUEST;
 assert(!a04_native_oid_async_init(&q,(void *)1,&c));assert(a04_native_oid_async_submit(&q,&r)==NDIS_STATUS_PENDING);
 assert(a04_native_oid_async_submit(&q,&r)==NDIS_STATUS_DOT11_MEDIA_IN_USE && freed==1);
 a04_native_oid_async_cancel(&q,&id);run();assert(completed==1 && last==NDIS_STATUS_REQUEST_ABORTED && !dispatched && freed==2);
 assert(a04_native_oid_async_submit(&q,&r)==NDIS_STATUS_PENDING);run();assert(completed==2 && last==NDIS_STATUS_SUCCESS && dispatched==1);
 assert(a04_native_oid_async_submit(&q,&r)==NDIS_STATUS_PENDING);assert(a04_native_oid_async_close(&q,1)==NDIS_STATUS_PENDING);
 run();assert(completed==3 && last==NDIS_STATUS_REQUEST_ABORTED);assert(a04_native_oid_async_close(&q,1)==NDIS_STATUS_SUCCESS);
 assert(!a04_native_oid_async_init(&q,(void *)1,&c));assert(a04_native_oid_async_submit(&q,&r)==NDIS_STATUS_PENDING);
 close_in_completion=&q;run();assert(!close_in_completion);assert(a04_native_oid_async_close(&q,1)==NDIS_STATUS_SUCCESS);
 puts("NDIS OID work item, cancellation and drain: OK");return 0;
}
