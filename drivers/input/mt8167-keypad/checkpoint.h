/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded first-only lifecycle evidence. No MMIO or pointer logging.
 * Own service NativeCheckpoint only; persistence is best effort, not a DDI result.
 * Header requires ntddk.h from its sole including TU (wdf_keys.c).
 */
#ifndef MT8167_KEYPAD_CHECKPOINT_H
#define MT8167_KEYPAD_CHECKPOINT_H
static WCHAR keys_checkpoint_path[512];
static UNICODE_STRING keys_checkpoint_service;
static LARGE_INTEGER keys_checkpoint_epoch;
static volatile LONG keys_checkpoint_claims[32];
enum {KEYS_CP_UNOBSERVED=0,KEYS_CP_NTSTATUS=1,KEYS_CP_BOOLEAN=2,KEYS_CP_BACKEND=3};
enum {KEYS_CP_ENTRY=1,KEYS_CP_DRIVER_RESULT=2,KEYS_CP_ADD=3,KEYS_CP_ADD_RESULT=4,
 KEYS_CP_PREPARE=5,KEYS_CP_RAW=6,KEYS_CP_TRANSLATED=7,KEYS_CP_INTERFACE=8,
 KEYS_CP_PREPARE_RESULT=9,KEYS_CP_D0=10,KEYS_CP_SCAN=11,KEYS_CP_D0_RESULT=12,
 KEYS_CP_PMIC_FAILURE=13,KEYS_CP_KEYPAD_FAILURE=14,KEYS_CP_HID_DESCRIPTOR=15,
 KEYS_CP_REPORT_DESCRIPTOR=16,KEYS_CP_READ_REQUEST=17,KEYS_CP_READ_DELIVERY=18,
 KEYS_CP_D0_EXIT=19,KEYS_CP_RELEASE=20,KEYS_CP_GET_INPUT=21,
 KEYS_CP_ATTRIBUTES=22,KEYS_CP_READ_FAILURE=23,KEYS_CP_FORWARD_FAILURE=24};
static NTSTATUS KeysCheckpointValue(PCWSTR label,const ULONG *data,ULONG bytes) {
 OBJECT_ATTRIBUTES oa;UNICODE_STRING name;HANDLE service=NULL,parameters=NULL,key=NULL;NTSTATUS s;
 InitializeObjectAttributes(&oa,&keys_checkpoint_service,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 s=ZwOpenKey(&service,KEY_CREATE_SUB_KEY,&oa);if(!NT_SUCCESS(s))return s;
 RtlInitUnicodeString(&name,L"Parameters");InitializeObjectAttributes(&oa,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,service,NULL);
 s=ZwCreateKey(&parameters,KEY_CREATE_SUB_KEY,&oa,0,NULL,REG_OPTION_NON_VOLATILE,NULL);
 if(NT_SUCCESS(s)){
  RtlInitUnicodeString(&name,L"NativeCheckpoint");InitializeObjectAttributes(&oa,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,parameters,NULL);
  s=ZwCreateKey(&key,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,NULL);
  if(NT_SUCCESS(s)){RtlInitUnicodeString(&name,label);s=ZwSetValueKey(key,&name,0,REG_BINARY,(PVOID)data,bytes);ZwClose(key);}
  ZwClose(parameters);
 }
 ZwClose(service);return s;
}
static void KeysCheckpointBegin(PUNICODE_STRING path) {
 ULONG epoch[4];
 if(!path||!path->Buffer||(path->Length&1u)||path->Length>sizeof(keys_checkpoint_path)-sizeof(WCHAR))return;
 RtlCopyMemory(keys_checkpoint_path,path->Buffer,path->Length);keys_checkpoint_path[path->Length/sizeof(WCHAR)]=0;
 RtlInitUnicodeString(&keys_checkpoint_service,keys_checkpoint_path);KeQuerySystemTime(&keys_checkpoint_epoch);
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 epoch[0]=0x31424b41u;epoch[1]=1;epoch[2]=keys_checkpoint_epoch.LowPart;epoch[3]=(ULONG)keys_checkpoint_epoch.HighPart;
 (void)KeysCheckpointValue(L"LoadEpochV1",epoch,sizeof(epoch));
}
static void KeysCheckpoint(ULONG stage,ULONG kind,ULONG status,ULONG a,ULONG b,ULONG c,ULONG d,ULONG e,ULONG f) {
 /* 20 ULONG/80 bytes, no native-struct alignment dependency. Slots B01..B24.
  * PASSIVE + APC checks precede first claim. All call sites are outside locks.
  * A failed store is not retried by polling; absent/stale records stay unknown. */
 ULONG r[20]={0};WCHAR label[4]={L'B',L'0',L'0',0};LARGE_INTEGER now;
 if(!stage||stage>24||!keys_checkpoint_service.Length||KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 if(InterlockedCompareExchange(&keys_checkpoint_claims[stage],1,0)!=0)return;
 KeQuerySystemTime(&now);r[0]=0x31424b41u;r[1]=1;r[2]=sizeof(r);r[3]=stage;r[4]=kind;r[5]=status;
 r[6]=a;r[7]=b;r[8]=c;r[9]=d;r[10]=e;r[11]=f;
 r[12]=keys_checkpoint_epoch.LowPart;r[13]=(ULONG)keys_checkpoint_epoch.HighPart;r[14]=now.LowPart;r[15]=(ULONG)now.HighPart;
 label[1]=(WCHAR)(L'0'+stage/10);label[2]=(WCHAR)(L'0'+stage%10);
 (void)KeysCheckpointValue(label,r,sizeof(r));
}
#endif
