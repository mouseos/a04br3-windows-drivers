/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Non-operational MSDC2 scaffolding. No maps, SD commands or radio writes. */
#include <ntddk.h>
#include <wdf.h>
EVT_WDF_DRIVER_DEVICE_ADD WifiAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE WifiPrepare;
NTSTATUS WifiPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 UNREFERENCED_PARAMETER(dev);UNREFERENCED_PARAMETER(raw);UNREFERENCED_PARAMETER(translated);
 /* No approved owner for shared resources, power sequence or firmware. */
 return STATUS_DEVICE_NOT_READY;
}
NTSTATUS WifiAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS callbacks;WDFDEVICE dev;
 UNREFERENCED_PARAMETER(driver);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);callbacks.EvtDevicePrepareHardware=WifiPrepare;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&callbacks);
 return WdfDeviceCreate(&init,WDF_NO_OBJECT_ATTRIBUTES,&dev);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{ WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,WifiAdd);return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE); }
