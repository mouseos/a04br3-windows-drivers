/* SPDX-License-Identifier: GPL-2.0-only
 * Calibration transport diagnostic only. Does not enable MMIO, clocks,
 * temperature polling, power control or claim to provide thermal protection. */
#include <ntddk.h>
#include <wdf.h>
#include "thermal.h"
#include "../../system/mt8167-toprgu/thermal_stop.h"
#define THERMAL_VAR_NAME L"A04BR3ThermalCalibration"
typedef struct { ULONG version, d60, d61, d62; } BOOT_CAL;
typedef struct { struct a04_thermal_cal cal; BOOLEAN calibrated; WDFIOTARGET watchdog; } THERMAL_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(THERMAL_CONTEXT,ThermalContext)
EVT_WDF_DRIVER_DEVICE_ADD ThermalAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE ThermalPrepare;
EVT_WDF_DEVICE_D0_ENTRY ThermalD0;
EVT_WDF_DEVICE_D0_EXIT ThermalExit;
static const GUID calGuid={0x92d56a32,0xc847,0x4fdc,{0x83,0x62,0xf2,0x77,0xc7,0x10,0xa0,0x43}};
static int read_cal(void *p,unsigned index,uint32_t *value)
{ const BOOT_CAL *v=p; if(index==60)*value=v->d60; else if(index==61)*value=v->d61; else if(index==62)*value=v->d62; else return -1; return 0; }
NTSTATUS ThermalPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 static const ULONGLONG addresses[]={0x1100d000,0x11003000,0x10018000,0x10009000};
 ULONG mask=0,i,j; UNREFERENCED_PARAMETER(dev);UNREFERENCED_PARAMETER(raw);
 for(i=0;i<WdfCmResourceListGetCount(translated);i++) {
  PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
  if(!d || d->Type!=CmResourceTypeMemory || d->u.Memory.Length!=0x1000) return STATUS_DEVICE_CONFIGURATION_ERROR;
  for(j=0;j<RTL_NUMBER_OF(addresses);j++) if(d->u.Memory.Start.QuadPart==addresses[j]) break;
  if(j==RTL_NUMBER_OF(addresses) || (mask&(1u<<j))) return STATUS_DEVICE_CONFIGURATION_ERROR;
  mask|=1u<<j;
 }
 return mask==0x0fu ? STATUS_SUCCESS : STATUS_DEVICE_CONFIGURATION_ERROR;
}
static NTSTATUS WatchdogRequest(THERMAL_CONTEXT *c,ULONG code)
{
 WDF_REQUEST_SEND_OPTIONS options;
 WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
 WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(1));
 return WdfIoTargetSendInternalIoctlSynchronously(c->watchdog,NULL,code,NULL,NULL,&options,NULL);
}
/* For the eventual monitor; a success means reset was requested, not observed. */
NTSTATUS A04ThermalRequestStop(WDFDEVICE dev)
{ THERMAL_CONTEXT *c=ThermalContext(dev); return c->watchdog ? WatchdogRequest(c,A04_TOPRGU_THERMAL_HALT) : STATUS_DEVICE_NOT_READY; }
NTSTATUS ThermalD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE prior)
{
 THERMAL_CONTEXT *c=ThermalContext(dev); BOOT_CAL v; ULONG size=sizeof(v),attributes=0;
 UNICODE_STRING name=RTL_CONSTANT_STRING(THERMAL_VAR_NAME); GUID guid=calGuid; NTSTATUS status;
 UNREFERENCED_PARAMETER(prior);c->calibrated=FALSE; RtlZeroMemory(&v,sizeof(v));
 status=ExGetFirmwareEnvironmentVariable(&name,&guid,&v,&size,&attributes);
 if(!NT_SUCCESS(status) || size!=sizeof(v) || v.version!=1 || (attributes&7u)!=6u || a04_thermal_calibrate(&c->cal,read_cal,&v)) {
  DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THERM boot calibration unavailable or invalid\n");
  return STATUS_DEVICE_NOT_READY;
 }
 {
  DECLARE_CONST_UNICODE_STRING(path,A04_TOPRGU_THERMAL_NAME);
  WDF_IO_TARGET_OPEN_PARAMS open;
  WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,GENERIC_READ|GENERIC_WRITE);
  status=WdfIoTargetOpen(c->watchdog,&open);
  if(!NT_SUCCESS(status)) return status;
  status=WatchdogRequest(c,A04_TOPRGU_THERMAL_QUERY);
  if(!NT_SUCCESS(status)) { WdfIoTargetClose(c->watchdog); return status; }
 }
 c->calibrated=TRUE;
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THERM boot calibration accepted; measurement disabled\n");
 return STATUS_SUCCESS;
}
NTSTATUS ThermalExit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE next)
{ UNREFERENCED_PARAMETER(next);WdfIoTargetClose(ThermalContext(dev)->watchdog); RtlSecureZeroMemory(&ThermalContext(dev)->cal,sizeof(struct a04_thermal_cal)); ThermalContext(dev)->calibrated=FALSE;return STATUS_SUCCESS; }
NTSTATUS ThermalAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS p; WDF_OBJECT_ATTRIBUTES a;WDFDEVICE device; NTSTATUS status;
 UNREFERENCED_PARAMETER(driver);WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);
 p.EvtDevicePrepareHardware=ThermalPrepare;p.EvtDeviceD0Entry=ThermalD0;p.EvtDeviceD0Exit=ThermalExit;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,THERMAL_CONTEXT);
 status=WdfDeviceCreate(&init,&a,&device); if(!NT_SUCCESS(status)) return status;
 WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ParentObject=device;
 return WdfIoTargetCreate(device,&a,&ThermalContext(device)->watchdog);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{ WDF_DRIVER_CONFIG c;WDF_DRIVER_CONFIG_INIT(&c,ThermalAdd);return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE); }
