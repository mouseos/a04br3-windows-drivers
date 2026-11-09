/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded, optional startup evidence. No pointers, GPU controls or flush.
 * FILETIME/interrupt time correlate with same-boot ModuleIdentity/receipts.
 */
#ifndef A04_START_CHECKPOINT357_H
#define A04_START_CHECKPOINT357_H
#define GEO357_WORDS 192u
struct geo_start357_record {
 ULONG schema,bytes,kind,build,first_failure_id,status,valid_flags,last_step;
 ULONGLONG begin_filetime,begin_interrupt,end_filetime,end_interrupt;
 ULONG words[GEO357_WORDS];
};
static void Geo357Begin(struct geo_start357_record *r,ULONG kind){
 LARGE_INTEGER t;RtlZeroMemory(r,sizeof(*r));r->schema=1;r->bytes=sizeof(*r);r->kind=kind;r->build=357;
 KeQuerySystemTime(&t);r->begin_filetime=(ULONGLONG)t.QuadPart;r->begin_interrupt=KeQueryInterruptTime();
}
static NTSTATUS Geo357Fail(struct geo_start357_record *r,ULONG id,NTSTATUS s){
 if(!r->first_failure_id)r->first_failure_id=id;r->last_step=id;return s;
}
static void Geo357End(struct geo_start357_record *r,NTSTATUS s){
 LARGE_INTEGER t;r->status=(ULONG)s;KeQuerySystemTime(&t);r->end_filetime=(ULONGLONG)t.QuadPart;r->end_interrupt=KeQueryInterruptTime();
}
static void Geo357Store(const struct geo_start357_record *r,int display,int power){
 UNICODE_STRING gpu=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\StartCheckpoint357");
 UNICODE_STRING dis=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\MT8167DisplayTransport\\Parameters\\StartCheckpoint357");
 UNICODE_STRING native=RTL_CONSTANT_STRING(L"NativeOutputV1"),start=RTL_CONSTANT_STRING(L"DisplayStartV1"),d0=RTL_CONSTANT_STRING(L"PowerD0V1");
 OBJECT_ATTRIBUTES oa;HANDLE k=NULL;ULONG disposition;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 InitializeObjectAttributes(&oa,display?&dis:&gpu,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 if(NT_SUCCESS(ZwCreateKey(&k,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition))){
  (void)ZwSetValueKey(k,display?(power?&d0:&start):&native,0,REG_BINARY,(void *)r,sizeof(*r));ZwClose(k);
 }
 /* Deliberately no ZwFlushKey. Presence is optional; absence is not success. */
}
#endif
