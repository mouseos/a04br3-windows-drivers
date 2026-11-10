/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded provenance for the early graph retirement seen in run189.
 * No MMIO, scheduling changes, forced flush, pointers or success substitution.
 */
#ifndef GEO_DISPLAY_LIFECYCLE364_H
#define GEO_DISPLAY_LIFECYCLE364_H
struct geo_display_lifecycle364 {
 ULONG schema,bytes,kind,sequence,completed,status,argument0,argument1;
 ULONG before_state,after_state,before_started,after_started;
 ULONGLONG begin_filetime,begin_interrupt,end_filetime,end_interrupt;
};
C_ASSERT(sizeof(struct geo_display_lifecycle364)==80);
static LONG geo_display_lifecycle364_sequence[3];
static void GeoDisplayLifecycle364Store(const struct geo_display_lifecycle364 *r){
 UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\DisplayLifecycle364");
 static const PCWSTR names[]={L"StopDeviceV1",L"PowerStateV1",L"DrainForDisplayV1"};
 OBJECT_ATTRIBUTES oa;UNICODE_STRING name;HANDLE key=NULL;ULONG disposition;
 if(r->kind<1||r->kind>3||KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 InitializeObjectAttributes(&oa,&path,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 if(NT_SUCCESS(ZwCreateKey(&key,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition))){
  RtlInitUnicodeString(&name,names[r->kind-1]);
  (void)ZwSetValueKey(key,&name,0,REG_BINARY,(void *)r,sizeof(*r));ZwClose(key);
 }
}
static void GeoDisplayLifecycle364Begin(struct geo_display_lifecycle364 *r,ULONG kind,
 struct geo_render_device *d,ULONG argument0,ULONG argument1){
 LARGE_INTEGER now;RtlZeroMemory(r,sizeof(*r));r->schema=1;r->bytes=sizeof(*r);r->kind=kind;
 r->sequence=(ULONG)InterlockedIncrement(&geo_display_lifecycle364_sequence[kind-1]);
 r->status=0xffffffffu;r->argument0=argument0;r->argument1=argument1;
 r->before_state=d?(ULONG)InterlockedCompareExchange(&d->session.state,0,0):0xffffffffu;
 r->before_started=d?d->started:0xffffffffu;r->after_state=r->after_started=0xffffffffu;
 KeQuerySystemTime(&now);r->begin_filetime=(ULONGLONG)now.QuadPart;r->begin_interrupt=KeQueryInterruptTime();
 GeoDisplayLifecycle364Store(r);
}
static NTSTATUS GeoDisplayLifecycle364End(struct geo_display_lifecycle364 *r,struct geo_render_device *d,NTSTATUS status){
 LARGE_INTEGER now;r->completed=1;r->status=(ULONG)status;
 r->after_state=d?(ULONG)InterlockedCompareExchange(&d->session.state,0,0):0xffffffffu;
 r->after_started=d?d->started:0xffffffffu;KeQuerySystemTime(&now);
 r->end_filetime=(ULONGLONG)now.QuadPart;r->end_interrupt=KeQueryInterruptTime();
 GeoDisplayLifecycle364Store(r);return status;
}
#endif
