/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Offline integration probe, never binds an INF. Even if a future parent grants
 * ownership, Prepare fails without acquiring it; no MMIO, IRQ or NDIS is started. */
#include <ntddk.h>
#include <wdf.h>
#include "../../bus/mt8167-shared/wifi_interface.h"
EVT_WDF_DRIVER_DEVICE_ADD WifiProbeAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE WifiProbePrepare;
NTSTATUS WifiProbePrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 A04_SHARED_WIFI_INTERFACE iface;NTSTATUS status;
 UNREFERENCED_PARAMETER(raw);UNREFERENCED_PARAMETER(translated);
 RtlZeroMemory(&iface,sizeof(iface));
 status=WdfFdoQueryForInterface(dev,&A04SharedWifiInterfaceGuid,(PINTERFACE)&iface,
  sizeof(iface),A04_SHARED_WIFI_INTERFACE_VERSION,NULL);
 if(!NT_SUCCESS(status))return status;
 if(!iface.Header.InterfaceDereference || !iface.Acquire || !iface.ReleaseQuiescent || !iface.Live || !iface.UpdateShared || !iface.WriteShared) {
  if(iface.Header.InterfaceDereference)iface.Header.InterfaceDereference(iface.Header.Context);
  return STATUS_DEVICE_CONFIGURATION_ERROR;
 }
 /* Acquisition is deliberately not attempted until rollback and lifetime are implemented. */
 iface.Header.InterfaceDereference(iface.Header.Context);
 return STATUS_DEVICE_NOT_READY;
}
NTSTATUS WifiProbeAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS callbacks;WDFDEVICE dev;WDF_OBJECT_ATTRIBUTES attr;
 UNREFERENCED_PARAMETER(driver);WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);
 callbacks.EvtDevicePrepareHardware=WifiProbePrepare;WdfDeviceInitSetPnpPowerEventCallbacks(init,&callbacks);
 WDF_OBJECT_ATTRIBUTES_INIT(&attr);attr.ExecutionLevel=WdfExecutionLevelPassive;
 return WdfDeviceCreate(&init,&attr,&dev);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,WifiProbeAdd);return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);}
