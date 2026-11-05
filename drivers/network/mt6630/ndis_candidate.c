/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * NDIS adapter initialization and rundown. Hardware startup is not ready. */
#include <ntddk.h>
#include <ndis.h>
#if defined(A04_NDIS_NVRAM_PROBE) && A04_NDIS_NVRAM_PROBE
#include "windows_nvram.h"
#endif
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
#include "native_oid_async.h"
#include "../../bus/mt8167-shared/wifi_interface.h"
#endif
static NDIS_HANDLE g_driver;
/* The context retains the miniport handle until asynchronous OIDs drain.
 * Initialize currently fails before registering a usable adapter. */
struct a04_ndis_context { NDIS_HANDLE adapter;
#if defined(A04_NDIS_NVRAM_PROBE) && A04_NDIS_NVRAM_PROBE
 struct a04_windows_nvram_job nvram;NTSTATUS nvram_status;UCHAR calibration[512];
#endif
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
 struct a04_wmt_bridge bridge;
 struct a04_native_oid_context oid;
 struct a04_native_oid_async oid_async;
 A04_SHARED_WIFI_INTERFACE parent;
#endif
};
#define A04_NDIS_TAG 'n40A'
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
/* Query the PDO below NDIS, never the top of our own device stack. The bus
 * owns the returned interface reference. This query does not acquire a lease
 * or touch MMIO, clocks or power. */
static NTSTATUS a04_query_parent(struct a04_ndis_context *a)
{
 PDEVICE_OBJECT pdo=NULL;PIRP irp;PIO_STACK_LOCATION sp;KEVENT event;
 IO_STATUS_BLOCK ios;NTSTATUS status;
 NdisMGetDeviceProperty(a->adapter,&pdo,NULL,NULL,NULL,NULL);
 if(!pdo)return STATUS_NO_SUCH_DEVICE;
 KeInitializeEvent(&event,NotificationEvent,FALSE);
 RtlZeroMemory(&ios,sizeof(ios));
 irp=IoBuildSynchronousFsdRequest(IRP_MJ_PNP,pdo,NULL,0,NULL,&event,&ios);
 if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
 sp=IoGetNextIrpStackLocation(irp);sp->MinorFunction=IRP_MN_QUERY_INTERFACE;
 sp->Parameters.QueryInterface.InterfaceType=&A04SharedWifiInterfaceGuid;
 sp->Parameters.QueryInterface.Size=sizeof(a->parent);
 sp->Parameters.QueryInterface.Version=A04_SHARED_WIFI_INTERFACE_VERSION;
 sp->Parameters.QueryInterface.Interface=(PINTERFACE)&a->parent;
 sp->Parameters.QueryInterface.InterfaceSpecificData=NULL;
 status=IoCallDriver(pdo,irp);
 if(status==STATUS_PENDING){KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);status=ios.Status;}
 if(!NT_SUCCESS(status))return status;
 if(a->parent.Header.Size!=sizeof(a->parent) || a->parent.Header.Version!=A04_SHARED_WIFI_INTERFACE_VERSION ||
    !a->parent.Header.InterfaceDereference || !a->parent.Acquire || !a->parent.ReleaseQuiescent || !a->parent.Live || !a->parent.UpdateShared || !a->parent.WriteShared) {
  if(a->parent.Header.InterfaceDereference)a->parent.Header.InterfaceDereference(a->parent.Header.Context);
  RtlZeroMemory(&a->parent,sizeof(a->parent));return STATUS_INVALID_DEVICE_STATE;
 }
 return STATUS_SUCCESS;
}
#endif
/* Called at PASSIVE_LEVEL after NDIS has stopped issuing callbacks. An OID
 * completion can reenter NDIS: retain the entire context until it returns. */
#if defined(A04_NDIS_NVRAM_PROBE) && A04_NDIS_NVRAM_PROBE
static void a04_nvram_complete(void *context,NTSTATUS status,const UCHAR *data)
{
 struct a04_ndis_context *a=(struct a04_ndis_context *)context;
 a->nvram_status=status;
 if(NT_SUCCESS(status)&&data)RtlCopyMemory(a->calibration,data,sizeof(a->calibration));
}
#endif
static void a04_adapter_free(struct a04_ndis_context *a)
{
 if(!a)return;
#if defined(A04_NDIS_NVRAM_PROBE) && A04_NDIS_NVRAM_PROBE
 a04_windows_nvram_drain(&a->nvram);
 RtlSecureZeroMemory(a->calibration,sizeof(a->calibration));
#endif
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
 if(a->oid_async.initialized) {
  while(a04_native_oid_async_close(&a->oid_async,1000)==NDIS_STATUS_PENDING) { }
 }
 if(a->parent.Header.InterfaceDereference)
  a->parent.Header.InterfaceDereference(a->parent.Header.Context);
#endif
 NdisFreeMemory(a,0,0);
}
MINIPORT_INITIALIZE A04MiniportInitialize;
MINIPORT_HALT A04MiniportHalt;
MINIPORT_PAUSE A04MiniportPause;
MINIPORT_RESTART A04MiniportRestart;
MINIPORT_OID_REQUEST A04MiniportOid;
MINIPORT_SEND_NET_BUFFER_LISTS A04MiniportSend;
MINIPORT_RETURN_NET_BUFFER_LISTS A04MiniportReturn;
MINIPORT_CANCEL_SEND A04MiniportCancelSend;
MINIPORT_SHUTDOWN A04MiniportShutdown;
MINIPORT_CANCEL_OID_REQUEST A04MiniportCancelOid;
DRIVER_UNLOAD A04MiniportUnload;
NDIS_STATUS A04MiniportInitialize(NDIS_HANDLE adapter,NDIS_HANDLE context,PNDIS_MINIPORT_INIT_PARAMETERS params)
{
 struct a04_ndis_context *a;NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES attr;NDIS_STATUS status;
 UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(params);
 a=NdisAllocateMemoryWithTagPriority(adapter,sizeof(*a),A04_NDIS_TAG,NormalPoolPriority);
 if(!a)return NDIS_STATUS_RESOURCES;
 NdisZeroMemory(a,sizeof(*a));a->adapter=adapter;
 NdisZeroMemory(&attr,sizeof(attr));
 attr.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
 attr.Header.Revision=NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
 attr.Header.Size=NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
 attr.MiniportAdapterContext=a;attr.InterfaceType=NdisInterfaceInternal;
 status=NdisMSetMiniportAttributes(adapter,(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&attr);
 if(status!=NDIS_STATUS_SUCCESS)goto failed;
#if defined(A04_NDIS_NVRAM_PROBE) && A04_NDIS_NVRAM_PROBE
 /* Explicit diagnostic only: drain before rejecting this candidate. A real
  * miniport must keep the job alive and continue asynchronously. */
 status=a04_windows_nvram_init(&a->nvram,adapter,a04_nvram_complete,a);
 if(status!=NDIS_STATUS_SUCCESS)goto failed;
 status=a04_windows_nvram_start(&a->nvram);
 if(status!=NDIS_STATUS_PENDING)goto failed;
 a04_windows_nvram_drain(&a->nvram);
 DbgPrintEx(DPFLTR_IHVNETWORK_ID,DPFLTR_INFO_LEVEL,"A04 MT6630: runtime NVRAM status=%08lx\n",a->nvram_status);
#endif
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
 a->oid.bridge=&a->bridge;
 status=a04_native_oid_async_init(&a->oid_async,adapter,&a->oid);
 if(status!=NDIS_STATUS_SUCCESS)goto failed;
 if(!NT_SUCCESS(a04_query_parent(a))) {status=NDIS_STATUS_ADAPTER_NOT_READY;goto failed;}
#endif
 /* No general/native-802.11 attributes or initialized hardware exist yet.
  * NDIS requires full network-I/O readiness before initialization succeeds. */
 status=NDIS_STATUS_ADAPTER_NOT_READY;
failed:
 a04_adapter_free(a);
 return status;
}
VOID A04MiniportHalt(NDIS_HANDLE ctx,NDIS_HALT_ACTION action)
{UNREFERENCED_PARAMETER(action);a04_adapter_free((struct a04_ndis_context *)ctx);}
NDIS_STATUS A04MiniportPause(NDIS_HANDLE ctx,PNDIS_MINIPORT_PAUSE_PARAMETERS params) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(params);return NDIS_STATUS_FAILURE;}
NDIS_STATUS A04MiniportRestart(NDIS_HANDLE ctx,PNDIS_MINIPORT_RESTART_PARAMETERS params) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(params);return NDIS_STATUS_FAILURE;}
NDIS_STATUS A04MiniportOid(NDIS_HANDLE ctx,PNDIS_OID_REQUEST request)
{
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
 struct a04_ndis_context *adapter=(struct a04_ndis_context *)ctx;
 if(!adapter)return NDIS_STATUS_INVALID_DATA;
 return a04_native_oid_async_submit(&adapter->oid_async,request);
#else
 UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(request);return NDIS_STATUS_NOT_SUPPORTED;
#endif
}
VOID A04MiniportSend(NDIS_HANDLE ctx,PNET_BUFFER_LIST list,NDIS_PORT_NUMBER port,ULONG flags)
{
 struct a04_ndis_context *adapter=(struct a04_ndis_context *)ctx;
 PNET_BUFFER_LIST current;
 UNREFERENCED_PARAMETER(port);
 for(current=list;current;current=NET_BUFFER_LIST_NEXT_NBL(current))
  NET_BUFFER_LIST_STATUS(current)=NDIS_STATUS_FAILURE;
 NdisMSendNetBufferListsComplete(adapter->adapter,list,
  (flags & NDIS_SEND_FLAGS_DISPATCH_LEVEL) ? NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);
}
VOID A04MiniportReturn(NDIS_HANDLE ctx,PNET_BUFFER_LIST list,ULONG flags) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(list);UNREFERENCED_PARAMETER(flags);}
VOID A04MiniportCancelSend(NDIS_HANDLE ctx,PVOID id) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(id);}
VOID A04MiniportShutdown(NDIS_HANDLE ctx,NDIS_SHUTDOWN_ACTION action) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(action);}
VOID A04MiniportCancelOid(NDIS_HANDLE ctx,PVOID id)
{
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
 struct a04_ndis_context *adapter=(struct a04_ndis_context *)ctx;
 if(adapter)a04_native_oid_async_cancel(&adapter->oid_async,id);
#else
 UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(id);
#endif
}
VOID A04MiniportUnload(PDRIVER_OBJECT obj) {UNREFERENCED_PARAMETER(obj);if(g_driver)NdisMDeregisterMiniportDriver(g_driver);}
NTSTATUS DriverEntry(PDRIVER_OBJECT obj,PUNICODE_STRING path)
{
 NDIS_MINIPORT_DRIVER_CHARACTERISTICS c;NdisZeroMemory(&c,sizeof(c));
 c.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;c.Header.Revision=NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;c.Header.Size=NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
 c.MajorNdisVersion=6;c.MinorNdisVersion=30;c.MajorDriverVersion=0;c.MinorDriverVersion=1;
 c.InitializeHandlerEx=A04MiniportInitialize;c.HaltHandlerEx=A04MiniportHalt;c.UnloadHandler=A04MiniportUnload;c.PauseHandler=A04MiniportPause;c.RestartHandler=A04MiniportRestart;c.OidRequestHandler=A04MiniportOid;c.SendNetBufferListsHandler=A04MiniportSend;c.ReturnNetBufferListsHandler=A04MiniportReturn;c.CancelSendHandler=A04MiniportCancelSend;c.ShutdownHandlerEx=A04MiniportShutdown;c.CancelOidRequestHandler=A04MiniportCancelOid;
 return NdisMRegisterMiniportDriver(obj,path,NULL,&c,&g_driver);
}
