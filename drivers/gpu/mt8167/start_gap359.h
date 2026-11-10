/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded startup return evidence only; no pointers, MMIO or forced flush.
 * A missing/entry-only record is never a successful return. */
#ifndef GEO_START_GAP359_H
#define GEO_START_GAP359_H
struct geo_gap359_record {
 ULONG schema,bytes,kind,build,entered,completed,last_step,return_status;
 ULONGLONG begin_filetime,begin_interrupt,end_filetime,end_interrupt;
 ULONG status[16],value[64];
};
static void Geo359Store(const struct geo_gap359_record *r){
 UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\StartGap359");
 static const PCWSTR names[]={L"MiniportV1",L"BridgeV1",L"BddV1",L"TransportQueryV1"};
 UNICODE_STRING name;OBJECT_ATTRIBUTES oa;HANDLE k=NULL;ULONG disposition;
 if(r->kind<1||r->kind>4||KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 InitializeObjectAttributes(&oa,&path,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 if(NT_SUCCESS(ZwCreateKey(&k,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition))){
  RtlInitUnicodeString(&name,names[r->kind-1]);
  (void)ZwSetValueKey(k,&name,0,REG_BINARY,(void *)r,sizeof(*r));ZwClose(k);
 }
}
static void Geo359Begin(struct geo_gap359_record *r,ULONG kind){
 LARGE_INTEGER t;ULONG n;RtlZeroMemory(r,sizeof(*r));r->schema=1;r->bytes=sizeof(*r);r->kind=kind;r->build=359;
 r->return_status=0xffffffffu;for(n=0;n<16;n++)r->status[n]=0xffffffffu;
 KeQuerySystemTime(&t);r->begin_filetime=(ULONGLONG)t.QuadPart;r->begin_interrupt=KeQueryInterruptTime();Geo359Store(r);
}
static void Geo359Enter(struct geo_gap359_record *r,ULONG step){if(step<16){r->entered|=1u<<step;r->last_step=step;}}
static void Geo359Result(struct geo_gap359_record *r,ULONG step,NTSTATUS status){if(step<16){r->entered|=1u<<step;r->completed|=1u<<step;r->last_step=step;r->status[step]=(ULONG)status;}}
static NTSTATUS Geo359End(struct geo_gap359_record *r,NTSTATUS status){
 LARGE_INTEGER t;r->return_status=(ULONG)status;KeQuerySystemTime(&t);r->end_filetime=(ULONGLONG)t.QuadPart;r->end_interrupt=KeQueryInterruptTime();Geo359Store(r);return status;
}
#endif
