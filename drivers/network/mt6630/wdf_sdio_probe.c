/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Opt-in parent-lease rejection diagnostic. No MMIO, IRQ, firmware or SDIO.
 * A granted lease is immediately returned; it is never used to start Wi-Fi.
 */
#include <ntddk.h>
#include <wdf.h>
#include "../../bus/mt8167-shared/wifi_interface.h"
#ifndef A04_WIFI_WDF_LEASE_DIAGNOSTIC
#define A04_WIFI_WDF_LEASE_DIAGNOSTIC 0
#endif
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
typedef struct _WIFI_CONTEXT {
 A04_SHARED_WIFI_INTERFACE parent;
 int referenced;
 /* A provider contract violation or failed release must not be mistaken for
  * a clean stop. Keep the parent interface reference in that case. It pins
  * the object only, not PnP resources, and cannot make an unsafe lease safe. */
 int uncertain;
} WIFI_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WIFI_CONTEXT,WifiContext)
#endif
EVT_WDF_DRIVER_DEVICE_ADD WifiAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE WifiPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE WifiRelease;
EVT_WDF_DEVICE_D0_ENTRY WifiD0Entry;
EVT_WDF_DEVICE_D0_EXIT WifiD0Exit;
NTSTATUS WifiPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 UNREFERENCED_PARAMETER(raw);UNREFERENCED_PARAMETER(translated);
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
 WIFI_CONTEXT *c=WifiContext(dev);NTSTATUS status;
 if(c->referenced||c->uncertain)return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&c->parent,sizeof(c->parent));
 status=WdfFdoQueryForInterface(dev,&A04SharedWifiInterfaceGuid,(PINTERFACE)&c->parent,
  sizeof(c->parent),A04_SHARED_WIFI_INTERFACE_VERSION,NULL);
 if(!NT_SUCCESS(status))return status;
 if(!c->parent.Header.InterfaceDereference)return STATUS_DEVICE_CONFIGURATION_ERROR;
 c->referenced=1;
 if(c->parent.Header.Size<sizeof(c->parent)||c->parent.Header.Version!=A04_SHARED_WIFI_INTERFACE_VERSION||
    !c->parent.Acquire||!c->parent.ReleaseQuiescent||!c->parent.Live||!c->parent.UpdateShared||!c->parent.WriteShared)
  return STATUS_DEVICE_CONFIGURATION_ERROR;
 return STATUS_SUCCESS;
#else
 UNREFERENCED_PARAMETER(dev);return STATUS_DEVICE_NOT_READY;
#endif
}
NTSTATUS WifiD0Entry(WDFDEVICE dev,WDF_POWER_DEVICE_STATE previous)
{
 UNREFERENCED_PARAMETER(previous);
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
 WIFI_CONTEXT *c=WifiContext(dev);ULONG generation=0;NTSTATUS status;
 if(!c->referenced||c->uncertain)return STATUS_DEVICE_NOT_READY;
 status=c->parent.Acquire(c->parent.Header.Context,&generation);
 if(!NT_SUCCESS(status))return status; /* contract: failure has no effects */
 /* Nothing has touched hardware. Do not enable the data path while forced
  * removal and lease/MMIO/IRQ rundown are unproven. */
 if(!generation || !NT_SUCCESS(c->parent.ReleaseQuiescent(c->parent.Header.Context,generation)))
  c->uncertain=1;
 return STATUS_DEVICE_NOT_READY;
#else
 UNREFERENCED_PARAMETER(dev);return STATUS_DEVICE_NOT_READY;
#endif
}
NTSTATUS WifiD0Exit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE target)
{UNREFERENCED_PARAMETER(dev);UNREFERENCED_PARAMETER(target);return STATUS_SUCCESS;}
NTSTATUS WifiRelease(WDFDEVICE dev,WDFCMRESLIST translated)
{
 UNREFERENCED_PARAMETER(translated);
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
 WIFI_CONTEXT *c=WifiContext(dev);
 if(c->referenced&&!c->uncertain){c->parent.Header.InterfaceDereference(c->parent.Header.Context);c->referenced=0;}
#else
 UNREFERENCED_PARAMETER(dev);
#endif
 return STATUS_SUCCESS;
}
NTSTATUS WifiAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS callbacks;WDFDEVICE dev;WDF_OBJECT_ATTRIBUTES attr;
 UNREFERENCED_PARAMETER(driver);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);
 callbacks.EvtDevicePrepareHardware=WifiPrepare;callbacks.EvtDeviceReleaseHardware=WifiRelease;
 callbacks.EvtDeviceD0Entry=WifiD0Entry;callbacks.EvtDeviceD0Exit=WifiD0Exit;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&callbacks);
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,WIFI_CONTEXT);
#else
 WDF_OBJECT_ATTRIBUTES_INIT(&attr);
#endif
 attr.ExecutionLevel=WdfExecutionLevelPassive;
 return WdfDeviceCreate(&init,&attr,&dev);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,WifiAdd);return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);}
