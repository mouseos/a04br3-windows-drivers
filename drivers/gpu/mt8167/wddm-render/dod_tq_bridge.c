/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "dod_tq_bridge.h"
static VOID reference(PVOID context)
{InterlockedIncrement(&((struct geo_dod_tq_bridge *)context)->references);}
static VOID dereference(PVOID context)
{InterlockedDecrement(&((struct geo_dod_tq_bridge *)context)->references);}
static NTSTATUS copy_tile(PVOID context,const VOID *input,ULONG bytes,VOID *output,struct geo_dod_tq_receipt *receipt)
{
 struct geo_dod_tq_bridge *b=context;NTSTATUS status;
 if(!receipt)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(receipt,sizeof(*receipt));
 if(!b || !input || !output || input==output || bytes!=GEO_DOD_TQ_TILE_BYTES ||
    ((ULONG_PTR)input&3u) || ((ULONG_PTR)output&3u) ||
    (ULONG_PTR)input>MAXULONG_PTR-bytes || (ULONG_PTR)output>MAXULONG_PTR-bytes ||
    ((ULONG_PTR)input<(ULONG_PTR)output+bytes && (ULONG_PTR)output<(ULONG_PTR)input+bytes) ||
    KeGetCurrentIrql()!=PASSIVE_LEVEL || KeAreAllApcsDisabled())return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&b->gate,Executive,KernelMode,FALSE,NULL);
 if(!b->active || !b->backend)status=STATUS_DEVICE_NOT_READY;
 else {
  receipt->size=sizeof(*receipt);receipt->version=GEO_DOD_TQ_VERSION;
  status=b->backend(b->backend_context,input,bytes,output,receipt);
  /* Do not coerce PENDING/TIMEOUT or an empty backend proof into success. */
  if(status==STATUS_SUCCESS && !GeoDodTqReceiptValid(receipt))status=STATUS_IO_DEVICE_ERROR;
 }
 if(status!=STATUS_SUCCESS)RtlZeroMemory(receipt,sizeof(*receipt));
 KeReleaseMutex(&b->gate,FALSE);return status;
}
VOID GeoDodTqInitialize(struct geo_dod_tq_bridge *b)
{RtlZeroMemory(b,sizeof(*b));KeInitializeMutex(&b->gate,0);b->references=1;}
NTSTATUS GeoDodTqStart(struct geo_dod_tq_bridge *b,GEO_DOD_TQ_COPY backend,PVOID context)
{
 NTSTATUS status;
 if(!b || !backend || !context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&b->gate,Executive,KernelMode,FALSE,NULL);
 if(b->active || !GeoDodTqCanRemove(b))status=STATUS_DEVICE_BUSY;
 else{b->backend=backend;b->backend_context=context;b->active=TRUE;status=STATUS_SUCCESS;}
 KeReleaseMutex(&b->gate,FALSE);return status;
}
VOID GeoDodTqStop(struct geo_dod_tq_bridge *b)
{
 /* Caller is a PASSIVE miniport stop/power/remove path. Wait for every active
  * CopyTile before its native backend can be quiesced or deleted. */
 KeWaitForSingleObject(&b->gate,Executive,KernelMode,FALSE,NULL);
 b->active=FALSE;b->backend=NULL;b->backend_context=NULL;
 KeReleaseMutex(&b->gate,FALSE);
}
BOOLEAN GeoDodTqCanRemove(struct geo_dod_tq_bridge *b)
{return InterlockedCompareExchange(&b->references,0,0)==1;}
NTSTATUS GeoDodTqQuery(struct geo_dod_tq_bridge *b,PQUERY_INTERFACE q)
{
 struct geo_dod_tq_interface result;NTSTATUS status=STATUS_NOT_SUPPORTED;
 if(!b || !q || !q->InterfaceType)return STATUS_INVALID_PARAMETER;
 if(!RtlEqualMemory(q->InterfaceType,&GeoDodTqQueryGuid,sizeof(GUID)))return STATUS_NOT_SUPPORTED;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL || q->Version!=GEO_DOD_TQ_VERSION ||
    q->Size!=sizeof(result) || !q->Interface || q->InterfaceSpecificData)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&b->gate,Executive,KernelMode,FALSE,NULL);
 if(b->active && b->backend){
  RtlZeroMemory(&result,sizeof(result));
  result.Header.Size=sizeof(result);result.Header.Version=GEO_DOD_TQ_VERSION;
  result.Header.Context=b;result.Header.InterfaceReference=reference;result.Header.InterfaceDereference=dereference;
  result.capabilities=GEO_DOD_TQ_PRIVATE_BOUNCE;result.CopyTile=copy_tile;
  reference(b);RtlCopyMemory(q->Interface,&result,sizeof(result));status=STATUS_SUCCESS;
 }
 KeReleaseMutex(&b->gate,FALSE);return status;
}
