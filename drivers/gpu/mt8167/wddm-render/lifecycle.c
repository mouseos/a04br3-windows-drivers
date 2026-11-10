/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "lifecycle.h"
#include "display_adapter356.h"
#include "../vendor_m1_11/native_factory_checkpoint.h"
/* TDR boundary only: existing schema2 records, flushed without changing
 * stop status/proof. PASSIVE only; no MMIO or completion notification. */
static void tdr_checkpoint(ULONG stage,NTSTATUS status,ULONG a,ULONG b,ULONG c,ULONG d) {
 UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\FactoryCheckpoint");
 OBJECT_ATTRIBUTES oa;HANDLE key=NULL;
 GeoFactoryCheckpoint(stage,status,a,b,c,d);
 InitializeObjectAttributes(&oa,&path,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 if(NT_SUCCESS(ZwOpenKey(&key,KEY_SET_VALUE,&oa))){(void)ZwFlushKey(key);ZwClose(key);}
}
static LONG state(struct geo_owner_session *s){return InterlockedCompareExchange(&s->state,0,0);}
static void set_state(struct geo_owner_session *s,LONG value){InterlockedExchange(&s->state,value);}
static BOOLEAN allowed(struct geo_owner_session *s,UINT mode) {
 LONG value=state(s);return value==GEO_SESSION_ACTIVE||(mode==2&&value==GEO_SESSION_HALTED)||(mode==3&&value==GEO_SESSION_FAILED);
}
BOOLEAN GeoSessionEnter(struct geo_kmd_adapter *a,UINT mode) {
 struct geo_owner_session *s;UINT passive=mode==1||mode==2;
 if(!a||!(s=a->session)||mode>3||KeGetCurrentIrql()>(passive?PASSIVE_LEVEL:DISPATCH_LEVEL))return FALSE;
 if(!ExAcquireRundownProtection(&s->operations))return FALSE;
 if(!allowed(s,mode)){ExReleaseRundownProtection(&s->operations);return FALSE;}
 if(passive) {
  KeWaitForSingleObject(&s->passive_lock,Executive,KernelMode,FALSE,NULL);
  if(!allowed(s,mode)){KeReleaseMutex(&s->passive_lock,FALSE);ExReleaseRundownProtection(&s->operations);return FALSE;}
 }
 return TRUE;
}
void GeoSessionLeave(struct geo_kmd_adapter *a,UINT mode) {
 struct geo_owner_session *s=a->session;
 if(mode==1||mode==2)KeReleaseMutex(&s->passive_lock,FALSE);ExReleaseRundownProtection(&s->operations);
}
static void close_operations(struct geo_owner_session *s) {
 set_state(s,GEO_SESSION_TRANSITION);
 if(!s->rundown_closed){ExWaitForRundownProtectionRelease(&s->operations);s->rundown_closed=1;}
}
static void reopen(struct geo_owner_session *s,LONG value) {
 if(s->rundown_closed){ExReInitializeRundownProtection(&s->operations);s->rundown_closed=0;}
 set_state(s,value);
}
static BOOLEAN poison(void *context){struct geo_owner_session *s=context;s->adapter->engine.poisoned=1;return TRUE;}
static NTSTATUS failed(struct geo_owner_session *s,NTSTATUS status) {
 if(s->adapter->synchronize)s->adapter->synchronize(s->adapter->sync_context,poison,s);
 set_state(s,GEO_SESSION_FAILED);return status==STATUS_SUCCESS?STATUS_INVALID_DEVICE_STATE:status;
}
static int proof_valid(const struct geo_owner_proof *p,uint64_t epoch,int reset) {
 return p->size==sizeof(*p)&&p->version==2&&!p->reserved&&p->flags==GEO_PROOF_REQUIRED&&
  (reset?p->epoch>epoch:p->epoch==epoch);
}
struct retire_call {struct geo_owner_session *session;NTSTATUS status;uint64_t epoch;UINT reset;};
static BOOLEAN retire(void *context) {
 struct retire_call *c=context;c->status=geo_retire_after_owner_drain(&c->session->adapter->engine);
 if(c->status==STATUS_SUCCESS){
  if(c->reset){c->session->adapter->engine.submitted_ever=0;c->session->adapter->engine.completed_ever=0;
   c->session->adapter->engine.last_submitted=0;c->session->adapter->engine.last_completed=0;}
  c->session->lease.epoch=c->epoch;InterlockedExchange(&c->session->adapter->service_pending,0);}return TRUE;
}
static NTSTATUS apply_proof(struct geo_owner_session *s,const struct geo_owner_proof *p,int reset) {
 struct retire_call c;
 if(!proof_valid(p,s->lease.epoch,reset))return failed(s,STATUS_INVALID_DEVICE_STATE);
 /* Actual owner proof says no new ISR/callback can arrive. Drain previously
  * queued scheduler DPCs before retiring software tokens or lease state. */
 KeFlushQueuedDpcs();c.session=s;c.status=STATUS_UNSUCCESSFUL;c.epoch=p->epoch;c.reset=reset!=0;
 if(!s->adapter->synchronize(s->lease.cookie,retire,&c)||c.status!=STATUS_SUCCESS)return failed(s,c.status);
 GeoCollectPendingPayloads(s->adapter);return STATUS_SUCCESS;
}
NTSTATUS GeoStartInProcessOwnerSession(struct geo_owner_session *s,struct geo_kmd_adapter *a,const struct geo_owner_control_v2 *control,const DXGKRNL_INTERFACE *dxgk) {
 NTSTATUS status;
 if(!s||!a||!control||!dxgk||KeGetCurrentIrql()!=PASSIVE_LEVEL||s->initialized||a->session)return STATUS_INVALID_PARAMETER;
 /* Session and adapter are allocated/zeroed by actual miniport AddDevice. */
 KeInitializeMutex(&s->control_lock,0);KeInitializeMutex(&s->passive_lock,0);ExInitializeRundownProtection(&s->operations);
 s->initialized=1;s->adapter=a;a->session=s;set_state(s,GEO_SESSION_TRANSITION);
 s->control=*control;
 if(s->control.Interface.Size!=sizeof(s->control)||s->control.Interface.Version!=2||
    !s->control.Interface.Context||!s->control.Interface.InterfaceReference||!s->control.Interface.InterfaceDereference||
    !s->control.acquire||!s->control.quiesce||!s->control.reset||!s->control.restart||!s->control.set_power||
    !s->control.release||!s->control.interrupt_event||!s->control.service||!s->control.synchronize)return failed(s,STATUS_NOT_SUPPORTED);
 s->control.Interface.InterfaceReference(s->control.Interface.Context);s->interface_referenced=1;
 s->lease.size=sizeof(s->lease);s->lease.version=2;
 status=s->control.acquire(s->control.Interface.Context,&s->lease);
 /* An ambiguous cookie/result remains referenced; no speculative release. */
 if(status!=STATUS_SUCCESS)return failed(s,status);
 if(s->lease.size!=sizeof(s->lease)||s->lease.version!=2||!s->lease.cookie||!s->lease.epoch||
    s->lease.irq_route!=GEO_IRQ_DXGK_REGISTERED||s->lease.irq_level<=DISPATCH_LEVEL||s->lease.irq_level>HIGH_LEVEL)return failed(s,STATUS_NOT_SUPPORTED);
 status=geo_engine_init(&a->engine,&s->lease.ops);if(status!=STATUS_SUCCESS)return failed(s,status);
 a->synchronize=s->control.synchronize;a->sync_context=s->lease.cookie;
 status=GeoInitializeClearNotifications(a,dxgk,(KIRQL)s->lease.irq_level);
 if(status!=STATUS_SUCCESS)return failed(s,status);
 status=s->control.restart(s->lease.cookie,s->lease.epoch);
 if(status!=STATUS_SUCCESS)return failed(s,status);
 set_state(s,GEO_SESSION_ACTIVE);return STATUS_SUCCESS;
}
static NTSTATUS drain(struct geo_owner_session *s,uint32_t reason) {
 struct geo_owner_proof p;NTSTATUS status;
 close_operations(s);RtlZeroMemory(&p,sizeof(p));p.size=sizeof(p);p.version=2;
 status=s->control.quiesce(s->lease.cookie,s->lease.epoch,reason,&p);
 if(status!=STATUS_SUCCESS)return failed(s,status);
 return apply_proof(s,&p,0);
}
NTSTATUS GeoStopOwnerSession(struct geo_owner_session *s) {
 NTSTATUS status;
 if(!s||!s->initialized||!s->lease.cookie||!s->adapter->notifications_ready||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&s->control_lock,Executive,KernelMode,FALSE,NULL);
 status=drain(s,GEO_QUIESCE_STOP);
 if(status==STATUS_SUCCESS&&(s->adapter->engine.allocation_count||s->adapter->device_count||s->adapter->shared_resources)) {
  /* Real drain succeeded but live VidMm surfaces still own the lease. Allow
   * only unmap/destroy, then retry Stop. No successful release is reported. */
  reopen(s,GEO_SESSION_HALTED);status=STATUS_DEVICE_BUSY;
 }else if(status==STATUS_SUCCESS) {
  status=s->control.release(s->lease.cookie,s->lease.epoch);
  if(status==STATUS_SUCCESS) {
   s->lease.cookie=NULL;s->adapter->notifications_ready=0;
   s->control.Interface.InterfaceDereference(s->control.Interface.Context);
   s->interface_referenced=0;set_state(s,GEO_SESSION_STOPPED);
  }else status=failed(s,status);
 }
 KeReleaseMutex(&s->control_lock,FALSE);return status;
}
NTSTATUS GeoPowerOwnerSession(struct geo_owner_session *s,DEVICE_POWER_STATE power) {
 NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
 if(!s||!s->initialized||!s->lease.cookie||!s->adapter->notifications_ready||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(power!=PowerDeviceD0&&power!=PowerDeviceD3)return STATUS_NOT_SUPPORTED;
 KeWaitForSingleObject(&s->control_lock,Executive,KernelMode,FALSE,NULL);
 if(power==PowerDeviceD3&&state(s)==GEO_SESSION_ACTIVE) {
  status=drain(s,GEO_QUIESCE_POWER);
  if(status==STATUS_SUCCESS)status=s->control.set_power(s->lease.cookie,s->lease.epoch,power);
  if(status==STATUS_SUCCESS)set_state(s,GEO_SESSION_SUSPENDED);else status=failed(s,status);
 }else if(power==PowerDeviceD0&&state(s)==GEO_SESSION_SUSPENDED) {
  status=s->control.set_power(s->lease.cookie,s->lease.epoch,power);
  if(status==STATUS_SUCCESS)status=s->control.restart(s->lease.cookie,s->lease.epoch);
  if(status==STATUS_SUCCESS)reopen(s,GEO_SESSION_ACTIVE);else status=failed(s,status);
 }
 KeReleaseMutex(&s->control_lock,FALSE);return status;
}
NTSTATUS GeoResetOwnerSession(struct geo_owner_session *s) {
 NTSTATUS status;struct geo_owner_proof p;
 if(!s||!s->initialized||!s->lease.cookie||!s->adapter->notifications_ready||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 tdr_checkpoint(6,STATUS_PENDING,s->adapter->engine.last_submitted,s->adapter->engine.last_completed,s->adapter->engine.poisoned,(ULONG)state(s));
 KeWaitForSingleObject(&s->control_lock,Executive,KernelMode,FALSE,NULL);close_operations(s);
 RtlZeroMemory(&p,sizeof(p));p.size=sizeof(p);p.version=2;
 status=s->control.reset(s->lease.cookie,s->lease.epoch,&p);
 if(status==STATUS_SUCCESS)status=apply_proof(s,&p,1);else status=failed(s,status);
 if(status==STATUS_SUCCESS)set_state(s,GEO_SESSION_RESET);
 tdr_checkpoint(7,status,p.flags,(ULONG)p.epoch,s->adapter->engine.last_completed,(ULONG)state(s));
 KeReleaseMutex(&s->control_lock,FALSE);return status;
}
NTSTATUS GeoRestartOwnerSession(struct geo_owner_session *s) {
 NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
 if(!s||!s->initialized||!s->lease.cookie||!s->adapter->notifications_ready||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&s->control_lock,Executive,KernelMode,FALSE,NULL);
 if(state(s)==GEO_SESSION_RESET) {
  status=s->control.restart(s->lease.cookie,s->lease.epoch);
  if(status==STATUS_SUCCESS)reopen(s,GEO_SESSION_ACTIVE);else status=failed(s,status);
 }
 tdr_checkpoint(8,status,(ULONG)s->lease.epoch,(ULONG)state(s),s->adapter->engine.poisoned,0);
 KeReleaseMutex(&s->control_lock,FALSE);return status;
}
BOOLEAN GeoOwnerInterrupt(struct geo_owner_session *s,ULONG message) {
 struct geo_owner_irq_event event;NTSTATUS status;UINT i;BOOLEAN handled=FALSE;
 if(!s||!s->lease.cookie||!s->adapter->notifications_ready||KeGetCurrentIrql()!=s->adapter->owner_irql)return FALSE;
 for(i=0;i<GEO_MAX_JOBS;i++) {
  RtlZeroMemory(&event,sizeof(event));event.size=sizeof(event);event.version=2;
  status=s->control.interrupt_event(s->lease.cookie,message,&event);
  if(status==STATUS_NOT_FOUND)break;
  if(status==STATUS_PENDING) {
   handled=TRUE;
   if(event.size!=sizeof(event)||event.version!=2||event.reserved||event.job||event.visibility){poison(s);break;}
   if(event.epoch==s->lease.epoch){InterlockedOr(&s->adapter->service_pending,1);s->adapter->queue_dpc(s->adapter->dxgk_device);}
   break;
  }
  if(status!=STATUS_SUCCESS){poison(s);break;}
  handled=TRUE;
  if(event.size!=sizeof(event)||event.version!=2||event.reserved){poison(s);break;}
  if(event.epoch!=s->lease.epoch)continue; /* stale epoch cannot release a current job */
  status=GeoCompleteFromInterrupt(s->adapter,event.job,event.generation,event.status,event.visibility);
  if(status!=STATUS_SUCCESS&&status!=STATUS_NOT_FOUND)break;
 }
 return handled;
}
NTSTATUS GeoServiceOwnerWork(struct geo_owner_session *s) {
 NTSTATUS status;
 if(!s||!s->lease.cookie||!s->control.service||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 /* The work item's pre-acquired rundown reference prevents reset/power/stop
  * from changing epoch or releasing the native context while service runs. */
 status=s->control.service(s->lease.cookie,s->lease.epoch);
 InterlockedExchange(&s->last_service_status,status);
 if(status!=STATUS_SUCCESS)return failed(s,status);
 return status; /* Never invoke GeoCompleteFromInterrupt here. */
}
NTSTATUS GeoServiceSubmittedWork(struct geo_owner_session *s) {
 NTSTATUS status;
 if(!s||!s->lease.cookie||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 /* The queued worker already owns its mode0 rundown reference. Take the
  * same PASSIVE mutex as Render/paging/cancel across claim through finish,
  * without reacquiring rundown after a concurrent close has started. IRQ
  * service above remains outside this lock so a waiting paging callback can
  * still obtain the real completion that lets it release the mutex. */
 KeWaitForSingleObject(&s->passive_lock,Executive,KernelMode,FALSE,NULL);
 status=state(s)==GEO_SESSION_ACTIVE?GeoPrepareSubmittedWork(s->adapter):STATUS_SUCCESS;
 KeReleaseMutex(&s->passive_lock,FALSE);
 InterlockedExchange(&s->last_service_status,status);
 if(status!=STATUS_SUCCESS){
  if(s->adapter->synchronize)s->adapter->synchronize(s->adapter->sync_context,poison,s);
  /* A reset/power transition owns TRANSITION once it closes rundown. */
  InterlockedCompareExchange(&s->state,GEO_SESSION_FAILED,GEO_SESSION_ACTIVE);
 }
 return status;
}
NTSTATUS APIENTRY CALLBACK GeoResetFromTimeout(HANDLE adapter) {
 struct geo_kmd_adapter *a=adapter;NTSTATUS status;
 if(!a||!a->session)return STATUS_INVALID_DEVICE_STATE;
 if(A04_GE8300_UNIFIED_DISPLAY356){status=GeoDisplay356Restore(a);if(status!=STATUS_SUCCESS)return status;}
 status=GeoResetOwnerSession(a->session);
 if(status==STATUS_SUCCESS&&A04_GE8300_UNIFIED_DISPLAY356)status=GeoDisplay356ReleaseMaps(a);return status;
}
NTSTATUS APIENTRY CALLBACK GeoRestartFromTimeout(HANDLE adapter) {
 struct geo_kmd_adapter *a=adapter;return a&&a->session?GeoRestartOwnerSession(a->session):STATUS_INVALID_DEVICE_STATE;
}
