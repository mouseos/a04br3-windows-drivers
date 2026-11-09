/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed PASSIVE initialization evidence. No IRQ/MMIO probe or pointer logging.
 */
#ifndef GEO_NATIVE_FACTORY_CHECKPOINT_H
#define GEO_NATIVE_FACTORY_CHECKPOINT_H
struct geo_factory_record {ULONG schema,bytes,stage,status,a,b,c,d,time_low,time_high;};
static void GeoFactoryCheckpoint(ULONG stage,NTSTATUS status,ULONG a,ULONG b,ULONG c,ULONG d) {
 UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\FactoryCheckpoint");
 WCHAR label[4]={L'R',L'0',L'0',0};UNICODE_STRING name;OBJECT_ATTRIBUTES oa;HANDLE key=NULL;ULONG disposition;
 LARGE_INTEGER now;struct geo_factory_record r={2,sizeof(r),stage,(ULONG)status,a,b,c,d,0,0};
 if(stage>63||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 KeQuerySystemTime(&now);r.time_low=now.LowPart;r.time_high=(ULONG)now.HighPart;
 label[1]=(WCHAR)(L'0'+stage/10);label[2]=(WCHAR)(L'0'+stage%10);RtlInitUnicodeString(&name,label);
 InitializeObjectAttributes(&oa,&path,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 if(!NT_SUCCESS(ZwCreateKey(&key,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition)))return;
 (void)ZwSetValueKey(key,&name,0,REG_BINARY,&r,sizeof(r));
 /* First-copy watchdog evidence must survive a reset. These existing
  * PASSIVE-only phase records otherwise remain solely in the hive cache.
  * No register operation, timeout, ownership proof or success status changes.
  * Render/Paging/Patch and native drain/rebuild boundaries are the only
  * added flush sites; query/caps and ISR/DPC paths remain unflushed. */
 if(!NT_SUCCESS(status)||stage==5||stage==20||stage==21||stage==22||
    (stage>=34&&stage<=39))(void)ZwFlushKey(key);ZwClose(key);
}
#endif
