/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Root client controller lifetime, backed by the original NDIS/parent epoch.
 * No second SDIO map/acquire, no PCM/latency success callbacks. */
#include "combo_controller.h"
#include "broker_abi.h"
#include "hfp_target.h"
static void lock(struct a04_combo_controller *c){(void)WdfWaitLockAcquire(c->serial,NULL);}
static void unlock(struct a04_combo_controller *c){WdfWaitLockRelease(c->serial);}
static void reference(PVOID p){WdfObjectReference(((struct a04_combo_controller *)p)->root);}
static void dereference(PVOID p){WdfObjectDereference(((struct a04_combo_controller *)p)->root);}
static BOOLEAN live(PVOID p,ULONG generation)
{
 struct a04_combo_controller *c=p;BOOLEAN result=FALSE;
 if(!ExAcquireRundownProtection(&c->live_calls))return FALSE;
 if(generation&&generation==c->physical.generation&&c->holds>0&&c->running&&!c->removed&&!c->uncertain&&c->physical.Live)
  result=c->physical.Live(c->physical.Header.Context,generation)==1;
 ExReleaseRundownProtection(&c->live_calls);return result;
}
static NTSTATUS acquire(PVOID p,PULONG generation)
{
 struct a04_combo_controller *c=p;struct a04_bt_broker_state state={0};WDF_MEMORY_DESCRIPTOR output;ULONG_PTR used=0;NTSTATUS s;
 if(!generation||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*generation=0;
 s=WdfDeviceStopIdle(c->root,TRUE);if(!NT_SUCCESS(s))return s;
 lock(c);
 if(!c->running||c->removed||c->uncertain||c->holds==MAXLONG||!c->physical.Live||
    c->physical.Live(c->physical.Header.Context,c->physical.generation)!=1){unlock(c);WdfDeviceResumeIdle(c->root);return STATUS_DEVICE_NOT_READY;}
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&output,&state,sizeof(state));
 s=WdfIoTargetSendIoctlSynchronously(c->target,WDF_NO_HANDLE,A04_BT_BROKER_SCO_HOLD,NULL,&output,NULL,&used);
 if(s==STATUS_DEVICE_NOT_READY||s==STATUS_DEVICE_BUSY){unlock(c);WdfDeviceResumeIdle(c->root);return s;}
 /* A late cancellation or malformed completion may follow a real broker
  * hold. Retain D0+object and return a nonzero partial generation: no fiction
  * that a failed synchronous request necessarily had no physical effects. */
 reference(c);InterlockedIncrement(&c->holds);*generation=c->physical.generation;
 if(!NT_SUCCESS(s)||used!=sizeof(state)||state.version!=2||!state.ready||!state.pcm_command_applied||state.generation!=*generation){
  InterlockedExchange(&c->uncertain,1);if(NT_SUCCESS(s))s=STATUS_DATA_ERROR;
 }
 unlock(c);return s;
}
static NTSTATUS release(PVOID p,ULONG generation)
{
 struct a04_combo_controller *c=p;WDF_MEMORY_DESCRIPTOR input;ULONG_PTR used=0;NTSTATUS s;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(c);if(!generation||generation!=c->physical.generation||c->holds<=0||c->uncertain){unlock(c);return STATUS_INVALID_DEVICE_STATE;}
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&input,&generation,sizeof(generation));
 s=WdfIoTargetSendIoctlSynchronously(c->target,WDF_NO_HANDLE,A04_BT_BROKER_SCO_RELEASE,&input,NULL,NULL,&used);
 if(NT_SUCCESS(s)&&used)s=STATUS_DATA_ERROR;
 if(!NT_SUCCESS(s)){InterlockedExchange(&c->uncertain,1);unlock(c);return s;}
 InterlockedDecrement(&c->holds);unlock(c);WdfDeviceResumeIdle(c->root);dereference(c);return STATUS_SUCCESS;
}
static NTSTATUS match(PVOID p,PDEVICE_OBJECT target)
{
 struct a04_combo_controller *c=p;ULONG generation=0;NTSTATUS s=acquire(c,&generation),closed;
 if(!NT_SUCCESS(s))return s;
 s=a04_bt_hfp_match_radio(WdfDeviceWdmGetDeviceObject(c->radio),target);
 closed=release(c,generation);return NT_SUCCESS(closed)?s:closed;
}
static NTSTATUS get(PVOID p,struct A04_HFP_CONTROLLER *out)
{
 struct a04_combo_controller *c=p;if(!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(out,sizeof(*out));lock(c);
 if(!c->running||c->removed||c->uncertain){unlock(c);return STATUS_DEVICE_NOT_READY;}
 out->size=sizeof(*out);out->version=A04_SHARED_HFP_VERSION;out->context=c;
 out->controller=WdfDeviceWdmGetDeviceObject(c->radio);out->Reference=reference;out->Dereference=dereference;
 out->Acquire=acquire;out->Live=live;out->Release=release;out->MatchHfpTarget=match;
 /* NULL qualification is deliberate v2 absence, not an always-failing or
  * always-successful substitute for unknown FC72 PCM/clock and codec delay. */
 reference(c);unlock(c);return STATUS_SUCCESS;
}
NTSTATUS a04_combo_controller_init(struct a04_combo_controller *c,WDFDEVICE root,WDFDEVICE radio,WDFIOTARGET target,WDFWAITLOCK serial)
{
 struct a04_mt6630_controller_interface iface;WDF_QUERY_INTERFACE_CONFIG query;NTSTATUS s;
 RtlZeroMemory(c,sizeof(*c));c->root=root;c->radio=radio;c->target=target;c->serial=serial;ExInitializeRundownProtection(&c->live_calls);
 s=IoRegisterDeviceInterface(WdfDeviceWdmGetPhysicalDevice(root),&A04Mt6630ControllerReadyGuid,NULL,&c->ready_link);
 if(!NT_SUCCESS(s))return s;
 RtlZeroMemory(&iface,sizeof(iface));iface.Header.Size=sizeof(iface);iface.Header.Version=1;iface.Header.Context=c;
 iface.Header.InterfaceReference=reference;iface.Header.InterfaceDereference=dereference;iface.GetController=get;
 WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&A04Mt6630ControllerInterfaceGuid,NULL);
 return WdfDeviceAddQueryInterface(root,&query);
}
NTSTATUS a04_combo_controller_start_locked(struct a04_combo_controller *c)
{
 WDF_MEMORY_DESCRIPTOR output;ULONG_PTR used=0;NTSTATUS s;
 if(c->running||c->holds||c->uncertain||c->removed)return STATUS_INVALID_DEVICE_STATE;
 RtlZeroMemory(&c->physical,sizeof(c->physical));WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&output,&c->physical,sizeof(c->physical));
 s=WdfIoTargetSendIoctlSynchronously(c->target,WDF_NO_HANDLE,A04_BT_BROKER_GET_PHYSICAL_OWNER,NULL,&output,NULL,&used);
 if(!NT_SUCCESS(s)||used!=sizeof(c->physical)||c->physical.Header.Size!=sizeof(c->physical)||
    c->physical.Header.Version!=A04_BT_PHYSICAL_OWNER_VERSION||!c->physical.Header.InterfaceDereference||
    !c->physical.Header.InterfaceReference||!c->physical.Live||!c->physical.generation){
  if(c->physical.Header.Context&&c->physical.Header.InterfaceDereference)c->physical.Header.InterfaceDereference(c->physical.Header.Context);
  RtlZeroMemory(&c->physical,sizeof(c->physical));return NT_SUCCESS(s)?STATUS_DATA_ERROR:s;
 }
 if(c->live_closed){ExReInitializeRundownProtection(&c->live_calls);c->live_closed=FALSE;}
 InterlockedExchange(&c->running,1);s=IoSetDeviceInterfaceState(&c->ready_link,TRUE);
 if(NT_SUCCESS(s))c->ready=TRUE;
 else {(void)a04_combo_controller_stop_locked(c);}
 return s;
}
NTSTATUS a04_combo_controller_stop_locked(struct a04_combo_controller *c)
{
 NTSTATUS s;if(c->holds||c->uncertain)return STATUS_DEVICE_BUSY;
 if(c->ready){s=IoSetDeviceInterfaceState(&c->ready_link,FALSE);if(!NT_SUCCESS(s))return s;c->ready=FALSE;}
 InterlockedExchange(&c->running,0);
 if(!c->live_closed){ExWaitForRundownProtectionRelease(&c->live_calls);c->live_closed=TRUE;}
 if(c->physical.Header.Context&&c->physical.Header.InterfaceDereference)c->physical.Header.InterfaceDereference(c->physical.Header.Context);
 RtlZeroMemory(&c->physical,sizeof(c->physical));return STATUS_SUCCESS;
}
void a04_combo_controller_destroy(struct a04_combo_controller *c)
{
 /* Every hold also owns a WDF object reference. Normal destruction therefore
  * follows successful Stop; uncertain acquisitions pin the object instead. */
 if(c->ready)(void)IoSetDeviceInterfaceState(&c->ready_link,FALSE);
 if(c->ready_link.Buffer)RtlFreeUnicodeString(&c->ready_link);
}
