/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "checkpoint.h"
static HANDLE key;
static volatile LONG sequence;
struct record {ULONG size,schema,driver_revision,sequence,stage,status,complete,reserved;ULONGLONG time_100ns;};
void SdCheckpointInitialize(PUNICODE_STRING path)
{
 HANDLE service=NULL,parameters=NULL;OBJECT_ATTRIBUTES a;UNICODE_STRING name;NTSTATUS status;
 InitializeObjectAttributes(&a,path,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 status=ZwOpenKey(&service,KEY_CREATE_SUB_KEY,&a);if(!NT_SUCCESS(status))return;
 RtlInitUnicodeString(&name,L"Parameters");InitializeObjectAttributes(&a,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,service,NULL);
 status=ZwCreateKey(&parameters,KEY_CREATE_SUB_KEY,&a,0,NULL,REG_OPTION_NON_VOLATILE,NULL);
 if(NT_SUCCESS(status)){
  RtlInitUnicodeString(&name,L"NativeCheckpoint");InitializeObjectAttributes(&a,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,parameters,NULL);
  (void)ZwCreateKey(&key,KEY_SET_VALUE,&a,0,NULL,REG_OPTION_NON_VOLATILE,NULL);ZwClose(parameters);
 }
 ZwClose(service);
}
void SdCheckpoint(PCWSTR value,ULONG stage,NTSTATUS status,BOOLEAN complete)
{
 struct record r;UNICODE_STRING name;
 if(!key||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 RtlZeroMemory(&r,sizeof(r));r.size=sizeof(r);r.schema=1;r.driver_revision=6;r.sequence=(ULONG)InterlockedIncrement(&sequence);
 r.stage=stage;r.status=(ULONG)status;r.complete=complete;r.time_100ns=KeQueryInterruptTime();RtlInitUnicodeString(&name,value);
 (void)ZwSetValueKey(key,&name,0,REG_BINARY,&r,sizeof(r));
}
void SdCheckpointClose(void){if(key){ZwClose(key);key=NULL;}}
