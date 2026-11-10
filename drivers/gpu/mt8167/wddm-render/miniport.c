/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_binding.h"
#include "display_adapter356.h"
#include "../wddm-display/unified_display.h"
#include "kmd_objects.h"
#include "query_adapter.h"
#include "init_checkpoint.h"
#include "../start_gap359.h"
#include "../vendor_m1_11/native_factory_checkpoint.h"
#include "post_release_stack366.h"
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
 PIO_WORKITEM work;LONG work_busy;ULONG work_reasons;
};
static NTSTATUS add_device(PDEVICE_OBJECT pdo,void **out) {
 struct geo_render_device *d;
 if(!pdo||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 d=ExAllocatePoolWithTag(NonPagedPool,sizeof(*d),'mOeG');if(!d)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(d,sizeof(*d));ObReferenceObject(pdo);d->pdo=pdo;
 GeoInitializeFrontendMemory(&d->adapter);
 d->work=IoAllocateWorkItem(pdo);
 if(!d->work){ObDereferenceObject(pdo);ExFreePoolWithTag(d,'mOeG');return STATUS_INSUFFICIENT_RESOURCES;}
 *out=d;GeoInitCheckpoint(3,STATUS_SUCCESS);return STATUS_SUCCESS;
}
static NTSTATUS start_device(void *context,DXGK_START_INFO *start,DXGKRNL_INTERFACE *dxgk,ULONG *sources,ULONG *children) {
 struct geo_render_device *d=context;NTSTATUS status;struct geo_gap359_record gap;Geo359Begin(&gap,1);
 if(!d||!start||!dxgk||!sources||!children||d->started||d->native_created||
    KeGetCurrentIrql()!=PASSIVE_LEVEL||dxgk->Size<sizeof(*dxgk)||!dxgk->DxgkCbGetDeviceInformation)return Geo359End(&gap,STATUS_INVALID_PARAMETER);
 d->dxgk=*dxgk;RtlZeroMemory(&d->device_info,sizeof(d->device_info));
 status=dxgk->DxgkCbGetDeviceInformation(dxgk->DeviceHandle,&d->device_info);GeoInitCheckpoint(4,status);if(status!=STATUS_SUCCESS)return Geo359End(&gap,status);
 d->native.size=sizeof(d->native);d->native.version=2;
 status=GeoNativeOwnerCreateV3(d->pdo,dxgk,&d->device_info,&d->adapter.memory_api,&d->native);GeoInitCheckpoint(5,status);
 if(status!=STATUS_SUCCESS){if(d->native.native_context)d->native_created=1;return Geo359End(&gap,status);}
 d->native_created=1;
 if(d->native.size!=sizeof(d->native)||d->native.version!=2||!d->native.native_context)return Geo359End(&gap,STATUS_INVALID_DEVICE_STATE);
 status=GeoConfigureAperture(&d->adapter,&d->native.aperture);GeoInitCheckpoint(6,status);if(status!=STATUS_SUCCESS)return Geo359End(&gap,status);
 status=GeoStartInProcessOwnerSession(&d->session,&d->adapter,&d->native.control,dxgk);GeoInitCheckpoint(7,status);
 if(status!=STATUS_SUCCESS)return Geo359End(&gap,status); /* Ambiguous resources remain pinned. */
 status=GeoNativeOwnerGetDrawOpsV1(&d->native,&d->adapter.draw);GeoInitCheckpoint(8,status);
 if(status!=STATUS_SUCCESS)return Geo359End(&gap,status);
 if(d->adapter.draw.size!=sizeof(d->adapter.draw)||d->adapter.draw.version!=GEO_DRAW_ABI_VERSION||
    !d->adapter.draw.context||!d->adapter.draw.prepare_draw)return Geo359End(&gap,STATUS_INVALID_DEVICE_STATE);
 status=GeoNativeOwnerGetDrawOpsV2(&d->native,&d->adapter.draw_v2);GeoInitCheckpoint(9,status);
 if(status!=STATUS_SUCCESS)return Geo359End(&gap,status);
 if(d->adapter.draw_v2.size!=sizeof(d->adapter.draw_v2)||d->adapter.draw_v2.version!=2||
    !d->adapter.draw_v2.context||!d->adapter.draw_v2.prepare_draw)return Geo359End(&gap,STATUS_INVALID_DEVICE_STATE);
 Geo359Enter(&gap,1);status=GeoNativeOwnerGetDrawOpsV3(&d->native,&d->adapter.draw_v3);Geo359Result(&gap,1,status);
 gap.value[0]=d->adapter.draw_v3.size;gap.value[1]=d->adapter.draw_v3.version;gap.value[2]=!!d->adapter.draw_v3.context;gap.value[3]=!!d->adapter.draw_v3.prepare_draw;
 Geo359Enter(&gap,2);if(status!=STATUS_SUCCESS||d->adapter.draw_v3.size!=sizeof(d->adapter.draw_v3)||d->adapter.draw_v3.version!=3||!d->adapter.draw_v3.context||!d->adapter.draw_v3.prepare_draw)return Geo359End(&gap,STATUS_INVALID_DEVICE_STATE);
 Geo359Result(&gap,2,STATUS_SUCCESS);
 Geo359Enter(&gap,3);status=GeoNativeOwnerGetDrawOpsV4(&d->native,&d->adapter.draw_v4);Geo359Result(&gap,3,status);
 gap.value[4]=d->adapter.draw_v4.size;gap.value[5]=d->adapter.draw_v4.version;gap.value[6]=!!d->adapter.draw_v4.context;gap.value[7]=!!d->adapter.draw_v4.prepare_draw;
 Geo359Enter(&gap,4);if(status!=STATUS_SUCCESS||d->adapter.draw_v4.size!=sizeof(d->adapter.draw_v4)||d->adapter.draw_v4.version!=4||!d->adapter.draw_v4.context||!d->adapter.draw_v4.prepare_draw)return Geo359End(&gap,STATUS_INVALID_DEVICE_STATE);
 Geo359Result(&gap,4,STATUS_SUCCESS);
 Geo359Enter(&gap,5);status=GeoNativeOwnerGetDrawOpsV5(&d->native,&d->adapter.draw_v5);Geo359Result(&gap,5,status);
 gap.value[8]=d->adapter.draw_v5.size;gap.value[9]=d->adapter.draw_v5.version;gap.value[10]=!!d->adapter.draw_v5.context;gap.value[11]=!!d->adapter.draw_v5.prepare_draw;
 Geo359Enter(&gap,6);if(status!=STATUS_SUCCESS||d->adapter.draw_v5.size!=sizeof(d->adapter.draw_v5)||d->adapter.draw_v5.version!=5||!d->adapter.draw_v5.context||!d->adapter.draw_v5.prepare_draw)return Geo359End(&gap,STATUS_INVALID_DEVICE_STATE);
 Geo359Result(&gap,6,STATUS_SUCCESS);
 Geo359Result(&gap,0,STATUS_SUCCESS);*sources=0;*children=0;
 if(A04_GE8300_UNIFIED_DISPLAY356){Geo359Enter(&gap,7);status=GeoDisplay356Start(&d->adapter,d->pdo,start,dxgk,sources,children);Geo359Result(&gap,7,status);gap.value[12]=*sources;gap.value[13]=*children;if(status!=STATUS_SUCCESS)return Geo359End(&gap,status);}
 d->started=1;GeoInitCheckpoint(10,STATUS_SUCCESS);return Geo359End(&gap,STATUS_SUCCESS);
}
static NTSTATUS stop_device_impl(void *context) {
 struct geo_render_device *d=context;NTSTATUS status;
 if(!d||!d->started||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 if(A04_GE8300_UNIFIED_DISPLAY356){status=GeoDisplay356Restore(&d->adapter);if(status!=STATUS_SUCCESS)return status;}
 if(d->session.state!=GEO_SESSION_STOPPED){
  status=GeoStopOwnerSession(&d->session);if(status!=STATUS_SUCCESS)return status;
 }
 if(A04_GE8300_UNIFIED_DISPLAY356){
  status=GeoDisplay356ReleaseMaps(&d->adapter);if(status!=STATUS_SUCCESS)return status;
  status=GeoDisplay356Stop(&d->adapter);if(status!=STATUS_SUCCESS)return status;
 }
 if(InterlockedCompareExchange(&d->adapter.memory_record_count,0,0)||InterlockedCompareExchange(&d->adapter.memory_fault,0,0))return STATUS_DEVICE_BUSY;
 status=GeoNativeOwnerDelete(&d->native);if(status!=STATUS_SUCCESS)return status;
 d->native_created=0;d->started=0;RtlZeroMemory(&d->session,sizeof(d->session));
 RtlZeroMemory(&d->adapter,sizeof(d->adapter));GeoInitializeFrontendMemory(&d->adapter);return STATUS_SUCCESS;
}
/* The OS can retire a fully started adapter before the first user client.
 * Keep the actual callback result separate from successful StartDevice. */
#include "display_lifecycle364.h"
static NTSTATUS stop_device(void *context) {
 struct geo_render_device *d=context;struct geo_display_lifecycle364 record;NTSTATUS status;
 GeoDisplayLifecycle364Begin(&record,1,d,0,0);status=stop_device_impl(context);
 return GeoDisplayLifecycle364End(&record,d,status);
}
static NTSTATUS remove_device(void *context) {
 struct geo_render_device *d=context;
 if(!d||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(InterlockedCompareExchange(&d->adapter.memory_record_count,0,0)||InterlockedCompareExchange(&d->adapter.memory_fault,0,0)||d->native_created||d->started||d->adapter.shared_resources||(d->adapter.engine.allocation_count||d->adapter.device_count)||InterlockedCompareExchange(&d->work_busy,0,0))return STATUS_DEVICE_BUSY;
 IoFreeWorkItem(d->work);ObDereferenceObject(d->pdo);ExFreePoolWithTag(d,'mOeG');return STATUS_SUCCESS;
}
static BOOLEAN interrupt_routine(void *context,ULONG message) {
 struct geo_render_device *d=context;
 return d?GeoOwnerInterrupt(&d->session,message):FALSE;
}
static void queue_owner_work(struct geo_render_device *d);
static void owner_work(PDEVICE_OBJECT device,void *context) {
 struct geo_render_device *d=context;NTSTATUS status;UNREFERENCED_PARAMETER(device);
 status=(d->work_reasons&1)?GeoServiceOwnerWork(&d->session):STATUS_SUCCESS;
 if(status==STATUS_SUCCESS)status=GeoServiceSubmittedWork(&d->session);
 InterlockedExchange(&d->work_busy,0);
 if(status==STATUS_SUCCESS&&InterlockedCompareExchange(&d->adapter.service_pending,0,0))queue_owner_work(d);
 GeoSessionLeave(&d->adapter,0);
}
static void queue_owner_work(struct geo_render_device *d) {
 if(!InterlockedCompareExchange(&d->adapter.service_pending,0,0))return;
 if(!GeoSessionEnter(&d->adapter,0))return;
 if(InterlockedCompareExchange(&d->work_busy,1,0)){GeoSessionLeave(&d->adapter,0);return;}
 d->work_reasons=(ULONG)InterlockedExchange(&d->adapter.service_pending,0);
 /* Reference is acquired BEFORE queuing and released by worker, so a queued
  * but not yet executing callback cannot outlive owner/miniport teardown. */
 IoQueueWorkItem(d->work,owner_work,DelayedWorkQueue,d);
}
void GeoQueueSubmittedPrepare(struct geo_kmd_adapter *a) {
 /* Adapter is the documented offset-zero member. Submit has returned from
  * DxgkCbSynchronizeExecution to <=DISPATCH, where IoQueueWorkItem is legal.
  * Do not queue a synthetic display DPC/interrupt to start software work. */
 if(a&&KeGetCurrentIrql()<=DISPATCH_LEVEL)queue_owner_work((struct geo_render_device *)a);
}
static void dpc_routine(void *context) {
 struct geo_render_device *d=(struct geo_render_device *)context;if(d){GeoClearDpc(&d->adapter);queue_owner_work(d);}
}
static NTSTATUS power_state_impl(void *context,ULONG uid,DEVICE_POWER_STATE power,POWER_ACTION action) {
 struct geo_render_device *d=(struct geo_render_device *)context;UNREFERENCED_PARAMETER(action);
 /* Microsoft DxgkDdiSetPowerState DeviceUid: DISPLAY_ADAPTER_HW_ID=-1.
  * This render-only device has no monitor/child power operations. */
 if(!d)return STATUS_INVALID_PARAMETER;
 if(uid!=0xffffffffu)return A04_GE8300_UNIFIED_DISPLAY356?GeoDisplayPower(GeoUnifiedDisplayContext(&d->adapter),uid,power,action):STATUS_NOT_SUPPORTED;
 NTSTATUS status;
 if(A04_GE8300_UNIFIED_DISPLAY356&&power==PowerDeviceD3){status=GeoDisplay356Restore(&d->adapter);if(status!=STATUS_SUCCESS)return status;}
 status=GeoPowerOwnerSession(&d->session,power);if(status!=STATUS_SUCCESS)return status;
 if(A04_GE8300_UNIFIED_DISPLAY356){
  if(power==PowerDeviceD3){status=GeoDisplay356ReleaseMaps(&d->adapter);if(status!=STATUS_SUCCESS)return status;}
  return GeoDisplayPower(GeoUnifiedDisplayContext(&d->adapter),uid,power,action);
 }
 return status;
}
static NTSTATUS power_state(void *context,ULONG uid,DEVICE_POWER_STATE power,POWER_ACTION action) {
 struct geo_render_device *d=context;struct geo_display_lifecycle364 record;NTSTATUS status;
 /* Adapter UID is UINT_MAX; child UID zero must not be confused with it.
  * Pack only the documented power/action enums into the second argument. */
 GeoDisplayLifecycle364Begin(&record,2,d,uid,((ULONG)action<<16)|(ULONG)power);
 status=power_state_impl(context,uid,power,action);
 return GeoDisplayLifecycle364End(&record,d,status);
}
/* The RT8.1 ARM initialization gate requires these base callbacks even for
 * an adapter with no video sources or children. MOU0007 owns all scanout.
 * Never invent a connector, mode, EDID or a successful source operation. */
static NTSTATUS dispatch_io_request(void *const context,ULONG source,PVIDEO_REQUEST_PACKET packet) {
 UNREFERENCED_PARAMETER(source);
 if(!context||!packet||!packet->StatusBlock)return STATUS_INVALID_PARAMETER;
 packet->StatusBlock->Status=87u; /* VP_STATUS ERROR_INVALID_PARAMETER */
 packet->StatusBlock->Information=0;
 return STATUS_INVALID_PARAMETER;
}
/* WDDM 1.3 requires metadata for every real scheduling node. This adapter
 * exposes one 3D node and no video/presentation engine or proprietary flags. */
static NTSTATUS get_node_metadata(HANDLE context,UINT ordinal,DXGKARG_GETNODEMETADATA *metadata) {
 NTSTATUS status=STATUS_INVALID_PARAMETER;
 if(context&&metadata&&!ordinal&&KeGetCurrentIrql()==PASSIVE_LEVEL){
  RtlZeroMemory(metadata,sizeof(*metadata));metadata->EngineType=DXGK_ENGINE_TYPE_3D;status=STATUS_SUCCESS;
 }
 GeoFactoryCheckpoint(61,status,ordinal,NT_SUCCESS(status)?DXGK_ENGINE_TYPE_3D:0,0,0);
 return status;
}
/* Record the real OS query and result without changing acceptance or exposing
 * pointers. This also separates a post-StartDevice failure from owner startup. */
static NTSTATUS observed_query_adapter_info(HANDLE context,const DXGKARG_QUERYADAPTERINFO *query) {
 NTSTATUS status=GeoQueryAdapterInfo(context,query);ULONG type=query?(ULONG)query->Type:0xffffffffu;
 GeoFactoryCheckpoint(type<=20u?40u+type:63u,status,type,query?query->InputDataSize:0,query?query->OutputDataSize:0,0);
 if(NT_SUCCESS(status)&&type==DXGKQAITYPE_DRIVERCAPS&&query->OutputDataSize>=sizeof(DXGK_DRIVERCAPS)){
  const DXGK_DRIVERCAPS *caps=query->pOutputData;
  GeoFactoryCheckpoint(62,status,caps->SchedulingCaps.Value,(ULONG)caps->WDDMVersion,
   caps->GpuEngineTopology.NbAsymetricProcessingNodes,
   (caps->SupportNonVGA?1u:0u)|(caps->SupportPerEngineTDR?2u:0u)|(caps->FlipCaps.FlipOnVSyncMmIo?4u:0u));
 }
 return status;
}
DXGKDDI_PRESENT GeoPresent; /* Local linkage declaration; no shared layout/header change. */
/* RT8.1 requires Present/interrupt slots on render-only adapters. Only a
 * bounded offscreen real-GPU Blt is supported; no display/flip is advertised. */
static NTSTATUS APIENTRY render_present(HANDLE context,DXGKARG_PRESENT *present) {
 NTSTATUS status=GeoPresent(context,present);
 GeoFactoryCheckpoint(60,status,present?present->Flags.Value:0,0,0,0);return status;
}
static NTSTATUS APIENTRY control_display_interrupt(HANDLE context,DXGK_INTERRUPT_TYPE type,BOOLEAN enable) {
 NTSTATUS status=context?STATUS_NOT_SUPPORTED:STATUS_INVALID_PARAMETER;
 /* This controls display/vsync interrupts, not native DMA completion IRQs. */
 GeoFactoryCheckpoint(59,status,(ULONG)type,(ULONG)enable,0,0);return status;
}
static NTSTATUS query_child_relations(void *const context,PDXGK_CHILD_DESCRIPTOR children,ULONG bytes) {
 const struct geo_render_device *d=context;
 if(!d||!children||bytes!=sizeof(*children)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(!d->started)return STATUS_DEVICE_NOT_READY;
 /* StartDevice reports zero children; the caller supplies one terminator. */
 RtlZeroMemory(children,sizeof(*children));return STATUS_SUCCESS;
}
static NTSTATUS query_child_status(void *const context,PDXGK_CHILD_STATUS status,BOOLEAN nondestructive) {
 UNREFERENCED_PARAMETER(nondestructive);
 if(!context||!status)return STATUS_INVALID_PARAMETER;
 return STATUS_INVALID_PARAMETER; /* Every ChildUid is absent on this adapter. */
}
static NTSTATUS query_device_descriptor(void *const context,ULONG child,PDXGK_DEVICE_DESCRIPTOR descriptor) {
 UNREFERENCED_PARAMETER(child);
 if(!context||!descriptor)return STATUS_INVALID_PARAMETER;
 return STATUS_INVALID_PARAMETER; /* No output device, hence no EDID bytes. */
}
static void reset_display_to_vga(void *const context) {
 /* Any IRQL. This non-VGA render adapter owns no display or POST surface.
  * VGA restoration belongs to the separate MOU0007 display miniport. Do not
  * touch its scanout or invoke the PASSIVE-only GPU/TDR owner reset here. */
 UNREFERENCED_PARAMETER(context);
}
static void unload_driver(void) {
 /* DriverEntry allocates no pools/handles; per-device resources were already
  * retired by RemoveDevice. Clear only the driver-global checkpoint state. */
 RtlZeroMemory(geo_checkpoint_path_buffer,sizeof(geo_checkpoint_path_buffer));
 RtlZeroMemory(&geo_checkpoint_path,sizeof(geo_checkpoint_path));
 InterlockedExchange(&geo_checkpoint_reached,0);
}
static int render_contract_valid(const DRIVER_INITIALIZATION_DATA *p) {
 /* Required render operations cannot be replaced with NULL/success stubs.
  * Capability implementation must also validate every feature it advertises;
  * pointer presence alone is not evidence of shader/preemption support. */
 return p->Version==DXGKDDI_INTERFACE_VERSION&&p->DxgkDdiPreemptCommand&&
  !p->DxgkDdiSetVidPnSourceAddress&&!p->DxgkDdiCommitVidPn&&!p->DxgkDdiPresent&&
  !p->DxgkDdiSetPointerShape&&!p->DxgkDdiSetPointerPosition&&!p->DxgkDdiGetScanLine;
}
/* Evidence only: load-time identity from the actual DriverObject, not a
 * preferred base or a guessed module page. No render/ISR path calls this.
 * FILETIME + interrupt-time pair identifies this load within its boot when
 * correlated with the same boot's collector/event evidence, not old runs. */
struct geo_module_identity_v1 {
 ULONG schema,bytes,pointer_bits,reserved;
 ULONGLONG driver_start;ULONG driver_size,flags;
 LARGE_INTEGER system_time;ULONGLONG interrupt_time;
 CHAR runtime_source_base[48],build_id[48];
};
C_ASSERT(sizeof(struct geo_module_identity_v1)==144);
static void module_identity(PDRIVER_OBJECT driver)
{
 static const CHAR source[]="4a952787f1befccd5c580e826b109fb9decc9c80";
 static const CHAR build[]="GPU376-gdi-shadow-v1";
 struct geo_module_identity_v1 r={0};HANDLE key=NULL;UNICODE_STRING path,name;
 OBJECT_ATTRIBUTES oa;ULONG disposition;NTSTATUS status;
 if(!driver||KeGetCurrentIrql()!=PASSIVE_LEVEL||!driver->DriverStart||!driver->DriverSize)return;
 r.schema=1;r.bytes=sizeof(r);r.pointer_bits=sizeof(PVOID)*8;
 r.driver_start=(ULONGLONG)(ULONG_PTR)driver->DriverStart;r.driver_size=driver->DriverSize;r.flags=1;
 KeQuerySystemTime(&r.system_time);r.interrupt_time=KeQueryInterruptTime();
 RtlCopyMemory(r.runtime_source_base,source,sizeof(source));RtlCopyMemory(r.build_id,build,sizeof(build));
 RtlInitUnicodeString(&path,L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\NativeCheckpoint");
 InitializeObjectAttributes(&oa,&path,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 status=ZwCreateKey(&key,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition);
 if(!NT_SUCCESS(status))return;
 RtlInitUnicodeString(&name,L"ModuleIdentityV1");
 status=ZwSetValueKey(key,&name,0,REG_BINARY,&r,sizeof(r));
 if(NT_SUCCESS(status))(void)ZwFlushKey(key);
 ZwClose(key);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry) {
 DRIVER_INITIALIZATION_DATA table=geo_ge8300_render_contract;NTSTATUS status;
 GeoInitCheckpointPath(registry);GeoInitCheckpoint(0,STATUS_SUCCESS);module_identity(driver);
 if(A04_GE8300_UNIFIED_DISPLAY356)GeoPostStack366Initialize();
 if(!driver||!registry||!render_contract_valid(&table)){GeoInitCheckpoint(1,STATUS_INVALID_PARAMETER);return STATUS_INVALID_PARAMETER;}
 GeoInitCheckpoint(1,STATUS_SUCCESS);
 table.DxgkDdiAddDevice=add_device;table.DxgkDdiStartDevice=start_device;
 table.DxgkDdiStopDevice=stop_device;table.DxgkDdiRemoveDevice=remove_device;
 table.DxgkDdiSetPowerState=power_state;table.DxgkDdiInterruptRoutine=interrupt_routine;table.DxgkDdiDpcRoutine=dpc_routine;
 table.DxgkDdiQueryAdapterInfo=observed_query_adapter_info;table.DxgkDdiPreemptCommand=GeoPreemptCommand;
 table.DxgkDdiCancelCommand=GeoCancelCommand;
 table.DxgkDdiPresent=render_present;table.DxgkDdiControlInterrupt=control_display_interrupt;
 if(A04_GE8300_UNIFIED_DISPLAY356){
  status=GeoDisplayFillVidPnCallbacks(&table);if(status!=STATUS_SUCCESS)return status;
  table.DxgkDdiEscape=GeoDisplay356Observe;table.DxgkDdiGetScanLine=GeoDisplay356GetScanLine;table.DxgkDdiSetVidPnSourceAddress=GeoDisplay356SetSource;table.DxgkDdiControlInterrupt=GeoDisplay356ControlInterrupt;
 }
 table.DxgkDdiDescribeAllocation=GeoDescribeAllocation;table.DxgkDdiGetStandardAllocationDriverData=GeoGetStandardAllocationDriverData;
 table.DxgkDdiCollectDbgInfo=GeoCollectDbgInfo;
 table.DxgkDdiCreateDevice=GeoCreateDevice;table.DxgkDdiDestroyDevice=GeoDestroyDevice;
 table.DxgkDdiCreateContext=GeoCreateContext;table.DxgkDdiDestroyContext=GeoDestroyContext;
 table.DxgkDdiOpenAllocation=GeoOpenAllocation;table.DxgkDdiCloseAllocation=GeoCloseAllocation;
 table.DxgkDdiCreateAllocation=GeoCreateAllocation;table.DxgkDdiDestroyAllocation=GeoDestroyAllocation;
 table.DxgkDdiBuildPagingBuffer=GeoBuildPagingBuffer;table.DxgkDdiRender=GeoRender;table.DxgkDdiPatch=GeoPatch;
 table.DxgkDdiSubmitCommand=GeoSubmitCommand;table.DxgkDdiQueryCurrentFence=GeoQueryCurrentFence;
 table.DxgkDdiResetFromTimeout=GeoResetFromTimeout;table.DxgkDdiRestartFromTimeout=GeoRestartFromTimeout;
 if(!A04_GE8300_UNIFIED_DISPLAY356)table.DxgkDdiDispatchIoRequest=dispatch_io_request;
 table.DxgkDdiGetNodeMetadata=get_node_metadata;
 if(!A04_GE8300_UNIFIED_DISPLAY356)table.DxgkDdiQueryChildRelations=query_child_relations;
 if(!A04_GE8300_UNIFIED_DISPLAY356){table.DxgkDdiQueryChildStatus=query_child_status;table.DxgkDdiQueryDeviceDescriptor=query_device_descriptor;}
 if(!A04_GE8300_UNIFIED_DISPLAY356)table.DxgkDdiResetDevice=reset_display_to_vga;table.DxgkDdiUnload=unload_driver;
 /* RT8.1 FUN_0008775c checks this complete render contract separately
  * from DxgkInitialize's base callbacks. Do not retry a known NULL gate. */
 if(!table.DxgkDdiPresent||!table.DxgkDdiControlInterrupt||!table.DxgkDdiRender||
    !table.DxgkDdiPatch||!table.DxgkDdiCreateAllocation||!table.DxgkDdiDestroyAllocation)
  return STATUS_INVALID_PARAMETER;
 status=DxgkInitialize(driver,registry,&table);GeoInitCheckpoint(2,status);return status;
}

NTSTATUS GeoUnifiedDrainNativeForDisplay(struct geo_kmd_adapter *a){
 struct geo_render_device *d=(struct geo_render_device *)a;struct geo_display_lifecycle364 record;NTSTATUS status;
 GeoDisplayLifecycle364Begin(&record,3,d,0,0);
 status=d&&d->native_created?GeoNativeOwnerDrainForDisplay(&d->native):STATUS_DEVICE_NOT_READY;
 return GeoDisplayLifecycle364End(&record,d,status);
}
#if A04_GE8300_UNIFIED_DISPLAY356
void GeoUnifiedEmergencyRestore(HANDLE adapter){GeoDisplay356EmergencyRestore(adapter);}

#endif
