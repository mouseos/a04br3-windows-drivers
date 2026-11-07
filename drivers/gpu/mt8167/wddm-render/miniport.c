/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_binding.h"
#include "kmd_objects.h"
#include "query_adapter.h"
#ifndef GEO_LINKED_NATIVE_CORE
#error Build this wrapper only with the reviewed in-process native owner core
#endif
struct geo_render_device {
 struct geo_kmd_adapter adapter; /* offset zero: scheduler DDIs receive this */
 struct geo_owner_session session;
 struct geo_native_binding native;
 PDEVICE_OBJECT pdo;
 DXGKRNL_INTERFACE dxgk;
 DXGK_DEVICE_INFO device_info;
 UINT native_created,started;
 PIO_WORKITEM work;LONG work_busy;
};
static NTSTATUS add_device(PDEVICE_OBJECT pdo,void **out) {
 struct geo_render_device *d;
 if(!pdo||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 d=ExAllocatePoolWithTag(NonPagedPool,sizeof(*d),'mOeG');if(!d)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(d,sizeof(*d));ObReferenceObject(pdo);d->pdo=pdo;
 GeoInitializeFrontendMemory(&d->adapter);
 d->work=IoAllocateWorkItem(pdo);
 if(!d->work){ObDereferenceObject(pdo);ExFreePoolWithTag(d,'mOeG');return STATUS_INSUFFICIENT_RESOURCES;}
 *out=d;return STATUS_SUCCESS;
}
static NTSTATUS start_device(void *context,DXGK_START_INFO *start,DXGKRNL_INTERFACE *dxgk,ULONG *sources,ULONG *children) {
 struct geo_render_device *d=context;NTSTATUS status;
 if(!d||!start||!dxgk||!sources||!children||d->started||d->native_created||
    KeGetCurrentIrql()!=PASSIVE_LEVEL||dxgk->Size<sizeof(*dxgk)||!dxgk->DxgkCbGetDeviceInformation)return STATUS_INVALID_PARAMETER;
 d->dxgk=*dxgk;RtlZeroMemory(&d->device_info,sizeof(d->device_info));
 status=dxgk->DxgkCbGetDeviceInformation(dxgk->DeviceHandle,&d->device_info);if(status!=STATUS_SUCCESS)return status;
 d->native.size=sizeof(d->native);d->native.version=2;
 status=GeoNativeOwnerCreateV3(d->pdo,dxgk,&d->device_info,&d->adapter.memory_api,&d->native);
 if(status!=STATUS_SUCCESS){if(d->native.native_context)d->native_created=1;return status;}
 d->native_created=1;
 if(d->native.size!=sizeof(d->native)||d->native.version!=2||!d->native.native_context)return STATUS_INVALID_DEVICE_STATE;
 status=GeoConfigureAperture(&d->adapter,&d->native.aperture);if(status!=STATUS_SUCCESS)return status;
 status=GeoStartInProcessOwnerSession(&d->session,&d->adapter,&d->native.control,dxgk);
 if(status!=STATUS_SUCCESS)return status; /* Ambiguous resources remain pinned. */
 status=GeoNativeOwnerGetDrawOpsV1(&d->native,&d->adapter.draw);
 if(status!=STATUS_SUCCESS)return status;
 if(d->adapter.draw.size!=sizeof(d->adapter.draw)||d->adapter.draw.version!=GEO_DRAW_ABI_VERSION||
    !d->adapter.draw.context||!d->adapter.draw.prepare_draw)return STATUS_INVALID_DEVICE_STATE;
 *sources=0;*children=0;d->started=1;return STATUS_SUCCESS;
}
static NTSTATUS stop_device(void *context) {
 struct geo_render_device *d=context;NTSTATUS status;
 if(!d||!d->started||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 if(d->session.state!=GEO_SESSION_STOPPED){
  status=GeoStopOwnerSession(&d->session);if(status!=STATUS_SUCCESS)return status;
 }
 if(InterlockedCompareExchange(&d->adapter.memory_record_count,0,0)||InterlockedCompareExchange(&d->adapter.memory_fault,0,0))return STATUS_DEVICE_BUSY;
 status=GeoNativeOwnerDelete(&d->native);if(status!=STATUS_SUCCESS)return status;
 d->native_created=0;d->started=0;RtlZeroMemory(&d->session,sizeof(d->session));
 RtlZeroMemory(&d->adapter,sizeof(d->adapter));GeoInitializeFrontendMemory(&d->adapter);return STATUS_SUCCESS;
}
static NTSTATUS remove_device(void *context) {
 struct geo_render_device *d=context;
 if(!d||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(InterlockedCompareExchange(&d->adapter.memory_record_count,0,0)||InterlockedCompareExchange(&d->adapter.memory_fault,0,0)||d->native_created||d->started||(d->adapter.engine.allocation_count||d->adapter.device_count)||InterlockedCompareExchange(&d->work_busy,0,0))return STATUS_DEVICE_BUSY;
 IoFreeWorkItem(d->work);ObDereferenceObject(d->pdo);ExFreePoolWithTag(d,'mOeG');return STATUS_SUCCESS;
}
static BOOLEAN interrupt_routine(void *context,ULONG message) {
 struct geo_render_device *d=context;
 return d?GeoOwnerInterrupt(&d->session,message):FALSE;
}
static void queue_owner_work(struct geo_render_device *d);
static void owner_work(PDEVICE_OBJECT device,void *context) {
 struct geo_render_device *d=context;NTSTATUS status;UNREFERENCED_PARAMETER(device);
 status=GeoServiceOwnerWork(&d->session);
 InterlockedExchange(&d->work_busy,0);
 if(status==STATUS_SUCCESS&&InterlockedCompareExchange(&d->adapter.service_pending,0,0))queue_owner_work(d);
 GeoSessionLeave(&d->adapter,0);
}
static void queue_owner_work(struct geo_render_device *d) {
 if(!InterlockedCompareExchange(&d->adapter.service_pending,0,0))return;
 if(!GeoSessionEnter(&d->adapter,0))return;
 if(InterlockedCompareExchange(&d->work_busy,1,0)){GeoSessionLeave(&d->adapter,0);return;}
 InterlockedExchange(&d->adapter.service_pending,0);
 /* Reference is acquired BEFORE queuing and released by worker, so a queued
  * but not yet executing callback cannot outlive owner/miniport teardown. */
 IoQueueWorkItem(d->work,owner_work,DelayedWorkQueue,d);
}
static void dpc_routine(void *context) {
 struct geo_render_device *d=(struct geo_render_device *)context;if(d){GeoClearDpc(&d->adapter);queue_owner_work(d);}
}
static NTSTATUS power_state(void *context,ULONG uid,DEVICE_POWER_STATE power,POWER_ACTION action) {
 struct geo_render_device *d=(struct geo_render_device *)context;UNREFERENCED_PARAMETER(action);
 /* Microsoft DxgkDdiSetPowerState DeviceUid: DISPLAY_ADAPTER_HW_ID=-1.
  * This render-only device has no monitor/child power operations. */
 if(!d||uid!=0xffffffffu)return STATUS_NOT_SUPPORTED;
 return GeoPowerOwnerSession(&d->session,power);
}
static int render_contract_valid(const DRIVER_INITIALIZATION_DATA *p) {
 /* Required render operations cannot be replaced with NULL/success stubs.
  * Capability implementation must also validate every feature it advertises;
  * pointer presence alone is not evidence of shader/preemption support. */
 return p->Version==DXGKDDI_INTERFACE_VERSION&&p->DxgkDdiPreemptCommand&&
  !p->DxgkDdiSetVidPnSourceAddress&&!p->DxgkDdiCommitVidPn&&!p->DxgkDdiPresent&&
  !p->DxgkDdiSetPointerShape&&!p->DxgkDdiSetPointerPosition&&!p->DxgkDdiQueryChildRelations&&
  !p->DxgkDdiQueryChildStatus&&!p->DxgkDdiQueryDeviceDescriptor&&!p->DxgkDdiGetScanLine;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry) {
 DRIVER_INITIALIZATION_DATA table=geo_ge8300_render_contract;
 if(!driver||!registry||!render_contract_valid(&table))return STATUS_INVALID_PARAMETER;
 table.DxgkDdiAddDevice=add_device;table.DxgkDdiStartDevice=start_device;
 table.DxgkDdiStopDevice=stop_device;table.DxgkDdiRemoveDevice=remove_device;
 table.DxgkDdiSetPowerState=power_state;table.DxgkDdiInterruptRoutine=interrupt_routine;table.DxgkDdiDpcRoutine=dpc_routine;
 table.DxgkDdiQueryAdapterInfo=GeoQueryAdapterInfo;table.DxgkDdiPreemptCommand=GeoPreemptCommand;
 table.DxgkDdiCancelCommand=GeoCancelCommand;
 table.DxgkDdiDescribeAllocation=GeoDescribeAllocation;table.DxgkDdiGetStandardAllocationDriverData=GeoGetStandardAllocationDriverData;
 table.DxgkDdiCollectDbgInfo=GeoCollectDbgInfo;
 table.DxgkDdiCreateDevice=GeoCreateDevice;table.DxgkDdiDestroyDevice=GeoDestroyDevice;
 table.DxgkDdiCreateContext=GeoCreateContext;table.DxgkDdiDestroyContext=GeoDestroyContext;
 table.DxgkDdiOpenAllocation=GeoOpenAllocation;table.DxgkDdiCloseAllocation=GeoCloseAllocation;
 table.DxgkDdiCreateAllocation=GeoCreateAllocation;table.DxgkDdiDestroyAllocation=GeoDestroyAllocation;
 table.DxgkDdiBuildPagingBuffer=GeoBuildPagingBuffer;table.DxgkDdiRender=GeoRender;table.DxgkDdiPatch=GeoPatch;
 table.DxgkDdiSubmitCommand=GeoSubmitCommand;table.DxgkDdiQueryCurrentFence=GeoQueryCurrentFence;
 table.DxgkDdiResetFromTimeout=GeoResetFromTimeout;table.DxgkDdiRestartFromTimeout=GeoRestartFromTimeout;
 return DxgkInitialize(driver,registry,&table);
}
