/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed service-private registry checkpoint; PASSIVE_LEVEL only. The logger
 * never changes the miniport's status, OID ownership or cleanup decisions.
 * Records and writes are bounded. Zw registry I/O itself has no timeout API. */
#include "checkpoint.h"
#define CP_TAG 'p40A'
static KMUTEX registry_lock;
static int initialized;
static uint64_t current_generation[2];
static NTSTATUS lock(void)
{
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!initialized)return STATUS_INVALID_DEVICE_STATE;
 return KeWaitForSingleObject(&registry_lock,Executive,KernelMode,FALSE,NULL);
}
static void unlock(void){KeReleaseMutex(&registry_lock,FALSE);}
static void error(struct a04_checkpoint *c,NTSTATUS status)
{
 if(!NT_SUCCESS(status)&&!c->record->diagnostic_error_observed){
  c->record->diagnostic_error_observed=1;c->record->diagnostic_error=(ULONG)status;
 }
}
static NTSTATUS set(HANDLE key,PCWSTR name,ULONG type,const void *data,ULONG size)
{
 UNICODE_STRING n;RtlInitUnicodeString(&n,name);
 return ZwSetValueKey(key,&n,0,type,(PVOID)data,size);
}
static NTSTATUS store(struct a04_checkpoint *c)
{
 NTSTATUS status=set(c->key,c->channel==A04_CP_DRIVER?L"DriverEntry":L"Initialize",REG_BINARY,c->record,sizeof(*c->record));
 error(c,status);return status;
}
static NTSTATUS flush(struct a04_checkpoint *c)
{
 NTSTATUS status;
 if(c->record->flush_calls>=A04_CP_MAX_FLUSHES){error(c,STATUS_QUOTA_EXCEEDED);return STATUS_QUOTA_EXCEEDED;}
 c->record->flush_calls++;
 /* A registry-only observer cannot prove the flush after the last persisted
  * write completed. Never set a durable-success bit before that operation. */
 c->record->tail_flush_known=0;
 status=ZwFlushKey(c->key);c->record->flush_observed=1;c->record->flush_status=(ULONG)status;
 error(c,status);return status;
}
void a04_cp_initialize(void)
{
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 KeInitializeMutex(&registry_lock,0);current_generation[0]=current_generation[1]=0;initialized=1;
}
static NTSTATUS open_key(HANDLE *out)
{
 UNICODE_STRING n;OBJECT_ATTRIBUTES attributes;HANDLE service=NULL,parameters=NULL,checkpoint=NULL;NTSTATUS status;
 RtlInitUnicodeString(&n,L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Mt6630Runtime");
 InitializeObjectAttributes(&attributes,&n,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
 status=ZwOpenKey(&service,KEY_CREATE_SUB_KEY,&attributes);if(!NT_SUCCESS(status))return status;
 RtlInitUnicodeString(&n,L"Parameters");InitializeObjectAttributes(&attributes,&n,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,service,NULL);
 status=ZwCreateKey(&parameters,KEY_CREATE_SUB_KEY,&attributes,0,NULL,REG_OPTION_NON_VOLATILE,NULL);
 if(NT_SUCCESS(status)){
  RtlInitUnicodeString(&n,L"NativeCheckpoint");InitializeObjectAttributes(&attributes,&n,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,parameters,NULL);
  status=ZwCreateKey(&checkpoint,KEY_WRITE|KEY_QUERY_VALUE,&attributes,0,NULL,REG_OPTION_NON_VOLATILE,NULL);
  if(NT_SUCCESS(status))*out=checkpoint;
  ZwClose(parameters);
 }
 ZwClose(service);return status;
}
NTSTATUS a04_cp_begin(struct a04_checkpoint *c,ULONG channel,uint64_t parent)
{
 union {uint64_t align;UCHAR bytes[FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION,Data)+8];} buffer;
 KEY_VALUE_PARTIAL_INFORMATION *value=(void *)buffer.bytes;UNICODE_STRING n;ULONG returned=0;
 uint64_t generation=0;NTSTATUS status;LARGE_INTEGER time;
 RtlZeroMemory(c,sizeof(*c));
 if(channel>1||KeGetCurrentIrql()!=PASSIVE_LEVEL||!initialized)return STATUS_INVALID_DEVICE_STATE;
 c->record=ExAllocatePoolWithTag(PagedPool,sizeof(*c->record),CP_TAG);
 if(!c->record)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(c->record,sizeof(*c->record));c->channel=channel;
 status=open_key(&c->key);if(!NT_SUCCESS(status))goto failed;
 status=lock();if(!NT_SUCCESS(status))goto failed;
 RtlInitUnicodeString(&n,L"Generation");
 status=ZwQueryValueKey(c->key,&n,KeyValuePartialInformation,value,sizeof(buffer.bytes),&returned);
 if(status==STATUS_OBJECT_NAME_NOT_FOUND)status=STATUS_SUCCESS;
 else if(NT_SUCCESS(status)){
  if(value->Type!=REG_QWORD||value->DataLength!=8||returned<sizeof(buffer.bytes))status=STATUS_DATA_ERROR;
  else RtlCopyMemory(&generation,value->Data,8);
 }
 if(NT_SUCCESS(status)){
  if(generation==UINT64_MAX)status=STATUS_INTEGER_OVERFLOW;
  else {generation++;status=set(c->key,L"Generation",REG_QWORD,&generation,8);}
 }
 if(NT_SUCCESS(status)){
  struct a04_cp_record *r=c->record;r->magic=A04_CP_MAGIC;r->version=A04_CP_VERSION;r->bytes=sizeof(*r);r->channel=channel;r->driver_build=1009;
  r->generation=generation;r->parent_generation=parent;r->interrupt_time=KeQueryInterruptTime();KeQuerySystemTime(&time);r->system_time=(uint64_t)time.QuadPart;
  current_generation[channel]=generation;
  status=store(c);
  if(NT_SUCCESS(status)){status=flush(c);unlock();return status;}
 }
 unlock();
 if(NT_SUCCESS(status))return status;
failed:
 if(c->key)ZwClose(c->key);
 ExFreePoolWithTag(c->record,CP_TAG);RtlZeroMemory(c,sizeof(*c));return status;
}
NTSTATUS a04_cp_emit(struct a04_checkpoint *c,ULONG stage,ULONG phase,ULONG kind,ULONG raw,ULONG detail)
{
 NTSTATUS status;
 if(!c||!c->key||!c->record)return STATUS_INVALID_DEVICE_STATE;
 status=lock();if(!NT_SUCCESS(status))return status;
 if(current_generation[c->channel]!=c->record->generation){unlock();return STATUS_RETRY;}
 if(c->record->count<A04_CP_CAPACITY){
  struct a04_cp_event *e=&c->record->events[c->record->count++];
  e->stage=stage;e->phase=phase;e->kind=kind;e->observed=phase==A04_CP_BEGIN?0:1;e->raw=raw;e->detail=detail;
 }else {if(c->record->overflow){unlock();return STATUS_QUOTA_EXCEEDED;}c->record->overflow=1;error(c,STATUS_QUOTA_EXCEEDED);}
 status=store(c);
 /* Flush only the fixed risky boundaries, not every small registry write.
  * Persist entry before a potentially blocking init step. The next snapshot
  * stores this actual flush result; an absent result stays unobserved. */
 if(NT_SUCCESS(status)&&phase==A04_CP_BEGIN&&
  (stage==A04_CP_REGISTER_DRIVER||stage==A04_CP_PARENT_QUERY||stage==A04_CP_SESSION||
   stage==A04_CP_GENERAL_ATTR||stage==A04_CP_NATIVE_ATTR||stage==A04_CP_CLEANUP))status=flush(c);
 unlock();return status;
}
NTSTATUS a04_cp_finish(struct a04_checkpoint *c,ULONG stage,ULONG kind,ULONG result)
{
 NTSTATUS status,tail;
 if(!c||!c->key||!c->record)return STATUS_INVALID_DEVICE_STATE;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 c->record->return_prepared=1;
 (void)a04_cp_emit(c,stage,A04_CP_RETURN_PREPARED,kind,result,0);
 status=lock();if(!NT_SUCCESS(status))return status;
 if(current_generation[c->channel]==c->record->generation){
  /* Two bounded passes: flush the result, then persist its actual NTSTATUS
   * and flush that report. No assertion about the unrecordable tail flush. */
  status=flush(c);tail=store(c);if(!NT_SUCCESS(tail))status=tail;
  tail=flush(c);if(!NT_SUCCESS(tail))status=tail;
  if(!NT_SUCCESS(status)){
   /* One fault-report attempt, never an unbounded flush retry. */
   (void)store(c);(void)flush(c);
  }
 }else status=STATUS_RETRY;
 if(c->record->diagnostic_error_observed&&NT_SUCCESS(status))status=(NTSTATUS)c->record->diagnostic_error;
 unlock();ZwClose(c->key);ExFreePoolWithTag(c->record,CP_TAG);RtlZeroMemory(c,sizeof(*c));return status;
}
void a04_cp_trace(void *context,unsigned stage,unsigned phase,unsigned kind,ULONG raw,ULONG detail)
{(void)a04_cp_emit(context,stage,phase,kind,raw,detail);}
