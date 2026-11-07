/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One bounded fixture exercises the actual writer, not a replica decoder. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "checkpoint.h"
#define IO_ERROR ((NTSTATUS)0xc0000185u)
static int irql;static unsigned registry_calls,flushes,fail_flush_at,has_generation;
static uint64_t generation;static struct a04_cp_record persisted[4];
static void registry(void){assert(irql==PASSIVE_LEVEL);registry_calls++;}
int KeGetCurrentIrql(void){return irql;}
void KeInitializeMutex(KMUTEX *m,ULONG level){assert(!level);m->held=0;}
NTSTATUS KeWaitForSingleObject(void *p,int why,int mode,int alert,void *timeout)
{KMUTEX *m=p;(void)why;(void)mode;assert(!alert&&!timeout&&!m->held);m->held=1;return 0;}
int KeReleaseMutex(KMUTEX *m,int wait){assert(m->held&&!wait);m->held=0;return 0;}
void RtlInitUnicodeString(UNICODE_STRING *u,PCWSTR s){u->text=s;}
PVOID ExAllocatePoolWithTag(int pool,size_t n,ULONG tag){assert(pool==PagedPool);(void)tag;return calloc(1,n);}
void ExFreePoolWithTag(PVOID p,ULONG tag){(void)tag;free(p);}
uint64_t KeQueryInterruptTime(void){return 123456;}
void KeQuerySystemTime(LARGE_INTEGER *p){p->QuadPart=999999;}
NTSTATUS ZwOpenKey(HANDLE *h,ULONG access,OBJECT_ATTRIBUTES *a)
{registry();assert(access==KEY_CREATE_SUB_KEY&&!a->root);assert(!wcscmp(a->name->text,L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Mt6630Runtime"));*h=(HANDLE)1;return 0;}
NTSTATUS ZwCreateKey(HANDLE *h,ULONG access,OBJECT_ATTRIBUTES *a,ULONG title,UNICODE_STRING *c,ULONG options,ULONG *d)
{
 registry();assert(!title&&!c&&!options&&!d);assert(a->flags==(OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE));
 if(a->root==(HANDLE)1){assert(!wcscmp(a->name->text,L"Parameters")&&access==KEY_CREATE_SUB_KEY);*h=(HANDLE)2;}
 else{assert(a->root==(HANDLE)2&&!wcscmp(a->name->text,L"NativeCheckpoint")&&access==(KEY_WRITE|KEY_QUERY_VALUE));*h=(HANDLE)3;}return 0;
}
NTSTATUS ZwClose(HANDLE h){registry();assert(h);return 0;}
NTSTATUS ZwSetValueKey(HANDLE h,UNICODE_STRING *n,ULONG title,ULONG type,PVOID p,ULONG size)
{
 registry();assert(h==(HANDLE)3&&!title);
 if(!wcscmp(n->text,L"Generation")){assert(type==REG_QWORD&&size==8);memcpy(&generation,p,8);has_generation=1;}
 else {unsigned channel=!wcscmp(n->text,L"Initialize")?1:!wcscmp(n->text,L"Scan")?2:!wcscmp(n->text,L"RuntimeFailure")?3:0;assert(channel||!wcscmp(n->text,L"DriverEntry"));assert(type==REG_BINARY&&size==sizeof(struct a04_cp_record));memcpy(&persisted[channel],p,size);}
 return 0;
}
NTSTATUS ZwQueryValueKey(HANDLE h,UNICODE_STRING *n,ULONG kind,PVOID p,ULONG size,ULONG *returned)
{
 KEY_VALUE_PARTIAL_INFORMATION *v=p;registry();assert(h==(HANDLE)3&&!wcscmp(n->text,L"Generation")&&kind==KeyValuePartialInformation);
 if(!has_generation)return STATUS_OBJECT_NAME_NOT_FOUND;
 assert(size==FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION,Data)+8);v->Type=REG_QWORD;v->DataLength=8;memcpy(v->Data,&generation,8);*returned=size;return 0;
}
NTSTATUS ZwFlushKey(HANDLE h)
{registry();assert(h==(HANDLE)3);flushes++;return flushes==fail_flush_at?IO_ERROR:STATUS_SUCCESS;}
int main(void)
{
 struct a04_checkpoint a,b;unsigned before;
 a04_cp_initialize();assert(!a04_cp_begin(&a,A04_CP_DRIVER,0));assert(a.record->generation==1);
 assert(!a04_cp_emit(&a,A04_CP_REGISTER_DRIVER,A04_CP_BEGIN,A04_CP_UNKNOWN,0,0));
 assert(!persisted[0].events[0].observed&&!persisted[0].tail_flush_known);
 assert(!a04_cp_emit(&a,A04_CP_REGISTER_DRIVER,A04_CP_RESULT,A04_CP_NDIS,0xc001009au,0));
 assert(!a04_cp_finish(&a,A04_CP_DRIVER_RETURN,A04_CP_NDIS,0xc001009au));
 assert(persisted[0].events[1].observed&&persisted[0].events[1].raw==0xc001009au);
 assert(!persisted[0].tail_flush_known&&persisted[0].return_prepared);
 assert(!a04_cp_begin(&a,A04_CP_INITIALIZE,1));assert(a.record->generation==2&&a.record->parent_generation==1);
 assert(!a04_cp_emit(&a,A04_CP_PARENT_QUERY,A04_CP_RESULT,A04_CP_NT,0xc0000022u,0));
 irql=2;before=registry_calls;
 assert(a04_cp_emit(&a,A04_CP_NATIVE_ATTR,A04_CP_RESULT,A04_CP_NDIS,0,0)==STATUS_INVALID_DEVICE_STATE);
 assert(a04_cp_finish(&a,A04_CP_INIT_RETURN,A04_CP_NDIS,0)==STATUS_INVALID_DEVICE_STATE&&registry_calls==before);
 irql=0;assert(!a04_cp_finish(&a,A04_CP_INIT_RETURN,A04_CP_NDIS,0xc0010011u));
 assert(persisted[1].events[0].kind==A04_CP_NT&&persisted[1].events[0].raw==0xc0000022u);
 fail_flush_at=flushes+1;
 assert(a04_cp_begin(&a,A04_CP_INITIALIZE,1)==IO_ERROR&&a.record);
 assert(!a04_cp_emit(&a,A04_CP_CONTEXT,A04_CP_RESULT,A04_CP_OBSERVATION,1,0));
 assert(a04_cp_finish(&a,A04_CP_INIT_RETURN,A04_CP_NDIS,0)==IO_ERROR);
 assert(persisted[1].diagnostic_error_observed&&persisted[1].diagnostic_error==(ULONG)IO_ERROR);
 fail_flush_at=0;assert(!a04_cp_begin(&a,A04_CP_INITIALIZE,1));
 for(unsigned i=0;i<200;i++)(void)a04_cp_emit(&a,A04_CP_CONTEXT,A04_CP_RESULT,A04_CP_OBSERVATION,1,0);
 assert(a.record->count==A04_CP_CAPACITY&&a.record->overflow);
 assert(a04_cp_finish(&a,A04_CP_INIT_RETURN,A04_CP_NDIS,0)==STATUS_QUOTA_EXCEEDED);
 assert(persisted[1].flush_calls<=A04_CP_MAX_FLUSHES);
 assert(!a04_cp_begin(&a,A04_CP_INITIALIZE,1));assert(!a04_cp_begin(&b,A04_CP_INITIALIZE,1));
 before=registry_calls;assert(a04_cp_emit(&a,A04_CP_CONTEXT,A04_CP_RESULT,A04_CP_OBSERVATION,1,0)==STATUS_RETRY&&registry_calls==before);
 assert(a04_cp_finish(&a,A04_CP_INIT_RETURN,A04_CP_NDIS,0)==STATUS_RETRY);
 assert(!a04_cp_finish(&b,A04_CP_INIT_RETURN,A04_CP_NDIS,0));
 assert(!a04_cp_begin(&a,A04_CP_SCAN,2));assert(!a04_cp_emit(&a,A04_CP_RUNTIME_STATE,A04_CP_RESULT,A04_CP_OBSERVATION,1,83));
 assert(!a04_cp_begin(&b,A04_CP_RUNTIME_FAILURE,2));assert(!a04_cp_emit(&b,A04_CP_HOST_STATE,A04_CP_RESULT,A04_CP_BACKEND,83,4));
 assert(!a04_cp_finish(&a,A04_CP_RUNTIME_RESULT,A04_CP_NDIS,0xc0010011u));assert(!a04_cp_finish(&b,A04_CP_RUNTIME_RESULT,A04_CP_BACKEND,83));
 assert(persisted[2].channel==2&&persisted[3].channel==3&&persisted[2].driver_build==1013&&persisted[3].parent_generation==2);
 puts("Fixed registry checkpoint: real writer status/phase, generation, IRQL, flush failure and bounds: OK");return 0;
}
