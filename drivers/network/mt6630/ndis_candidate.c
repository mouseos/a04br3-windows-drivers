/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * NDIS integration scaffold only. No adapter or radio can be started. */
#include <ntddk.h>
#include <ndis.h>
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
#include "native_oid.h"
#endif
static NDIS_HANDLE g_driver;
/* Future adapter contexts must retain the NDIS miniport handle until all
 * sends are completed. Initialize currently always fails and creates none. */
struct a04_ndis_context { NDIS_HANDLE adapter;
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
 struct a04_native_oid_context oid;
#endif
};
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
{ UNREFERENCED_PARAMETER(adapter);UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(params);return NDIS_STATUS_FAILURE; }
VOID A04MiniportHalt(NDIS_HANDLE ctx,NDIS_HALT_ACTION action) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(action);}
NDIS_STATUS A04MiniportPause(NDIS_HANDLE ctx,PNDIS_MINIPORT_PAUSE_PARAMETERS params) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(params);return NDIS_STATUS_FAILURE;}
NDIS_STATUS A04MiniportRestart(NDIS_HANDLE ctx,PNDIS_MINIPORT_RESTART_PARAMETERS params) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(params);return NDIS_STATUS_FAILURE;}
NDIS_STATUS A04MiniportOid(NDIS_HANDLE ctx,PNDIS_OID_REQUEST request)
{
#if defined(A04_NDIS_OID_BRIDGE) && A04_NDIS_OID_BRIDGE
 struct a04_ndis_context *adapter=(struct a04_ndis_context *)ctx;
 if(!adapter)return NDIS_STATUS_INVALID_DATA;
 return a04_native_oid_dispatch(&adapter->oid,request);
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
VOID A04MiniportCancelOid(NDIS_HANDLE ctx,PVOID id) {UNREFERENCED_PARAMETER(ctx);UNREFERENCED_PARAMETER(id);}
VOID A04MiniportUnload(PDRIVER_OBJECT obj) {UNREFERENCED_PARAMETER(obj);if(g_driver)NdisMDeregisterMiniportDriver(g_driver);}
NTSTATUS DriverEntry(PDRIVER_OBJECT obj,PUNICODE_STRING path)
{
 NDIS_MINIPORT_DRIVER_CHARACTERISTICS c;NdisZeroMemory(&c,sizeof(c));
 c.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;c.Header.Revision=NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;c.Header.Size=NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
 c.MajorNdisVersion=6;c.MinorNdisVersion=30;c.MajorDriverVersion=0;c.MinorDriverVersion=1;
 c.InitializeHandlerEx=A04MiniportInitialize;c.HaltHandlerEx=A04MiniportHalt;c.UnloadHandler=A04MiniportUnload;c.PauseHandler=A04MiniportPause;c.RestartHandler=A04MiniportRestart;c.OidRequestHandler=A04MiniportOid;c.SendNetBufferListsHandler=A04MiniportSend;c.ReturnNetBufferListsHandler=A04MiniportReturn;c.CancelSendHandler=A04MiniportCancelSend;c.ShutdownHandlerEx=A04MiniportShutdown;c.CancelOidRequestHandler=A04MiniportCancelOid;
 return NdisMRegisterMiniportDriver(obj,path,NULL,&c,&g_driver);
}
