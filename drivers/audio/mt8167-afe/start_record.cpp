/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <ntddk.h>
#include "start_record.h"
#if A04_AUDIO_START_PERSIST
NTSTATUS Mt8167PersistStartRecord(const Mt8167AudioStartRecord *record)
{
 if(!record||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 UNICODE_STRING key,name;OBJECT_ATTRIBUTES attributes;HANDLE handle=NULL;ULONG disposition,complete=0;
 RtlInitUnicodeString(&key,L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Mt8167Audio\\Parameters");
 InitializeObjectAttributes(&attributes,&key,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
 NTSTATUS status=ZwCreateKey(&handle,KEY_SET_VALUE,&attributes,0,NULL,REG_OPTION_NON_VOLATILE,&disposition);
 if(!NT_SUCCESS(status))return status;
 RtlInitUnicodeString(&name,L"NativeStartComplete");status=ZwSetValueKey(handle,&name,0,REG_DWORD,&complete,sizeof(complete));
 if(NT_SUCCESS(status)){
  RtlInitUnicodeString(&name,L"NativeStart");status=ZwSetValueKey(handle,&name,0,REG_BINARY,(PVOID)record,sizeof(*record));
 }
 if(NT_SUCCESS(status)){
  complete=1;RtlInitUnicodeString(&name,L"NativeStartComplete");status=ZwSetValueKey(handle,&name,0,REG_DWORD,&complete,sizeof(complete));
 }
 /* Once after the fixed, bounded snapshot; not an IRQ/event logger. */
 NTSTATUS flushed=ZwFlushKey(handle);if(NT_SUCCESS(status))status=flushed;
 NTSTATUS closed=ZwClose(handle);if(NT_SUCCESS(status))status=closed;return status;
}
#endif
