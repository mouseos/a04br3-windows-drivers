/* SPDX-License-Identifier: GPL-2.0-only
 * Calibration transport diagnostic only. Does not enable MMIO, clocks,
 * temperature polling, power control or claim to provide thermal protection. */
#include <ntddk.h>
#include <wdf.h>
#include "thermal.h"
#include "manager.h"
#include "ownership.h"
#include "../../system/mt8167-toprgu/thermal_stop.h"
#ifndef A04_THERMAL_LEASE_DIAG
#define A04_THERMAL_LEASE_DIAG 0
#endif
#if A04_THERMAL_LEASE_DIAG
#include "../../input/bu21025/thermal_broker.h"
#endif
#define THERMAL_VAR_NAME L"A04BR3ThermalCalibration"
typedef struct { ULONG version, d60, d61, d62; } BOOT_CAL;
typedef struct { struct a04_thermal_cal cal; struct a04_thermal_owner ownership; BOOLEAN calibrated, pnp_resources; WDFIOTARGET watchdog;
#if A04_THERMAL_LEASE_DIAG
 WDFIOTARGET touch; BOOLEAN lease_owned;
#endif
} THERMAL_CONTEXT;
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
 ULONG mask=0,i,j; THERMAL_CONTEXT *c=ThermalContext(dev); UNREFERENCED_PARAMETER(raw);
 c->pnp_resources=FALSE;
 for(i=0;i<WdfCmResourceListGetCount(translated);i++) {
  PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
  if(!d || d->Type!=CmResourceTypeMemory || d->u.Memory.Length!=0x1000) return STATUS_DEVICE_CONFIGURATION_ERROR;
  for(j=0;j<RTL_NUMBER_OF(addresses);j++) if(d->u.Memory.Start.QuadPart==addresses[j]) break;
  if(j==RTL_NUMBER_OF(addresses) || (mask&(1u<<j))) return STATUS_DEVICE_CONFIGURATION_ERROR;
  mask|=1u<<j;
 }
 if(mask!=0x0fu) return STATUS_DEVICE_CONFIGURATION_ERROR;
 c->pnp_resources=TRUE; return STATUS_SUCCESS;
}
static NTSTATUS WatchdogRequest(THERMAL_CONTEXT *c,ULONG code)
{
 WDF_REQUEST_SEND_OPTIONS options;
 WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
 WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(1));
 return WdfIoTargetSendInternalIoctlSynchronously(c->watchdog,NULL,code,NULL,NULL,&options,NULL);
}
#if A04_THERMAL_LEASE_DIAG
static NTSTATUS TouchRequest(THERMAL_CONTEXT *c,ULONG code)
{
 WDF_REQUEST_SEND_OPTIONS options;
 WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
 WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(1));
 return WdfIoTargetSendInternalIoctlSynchronously(c->touch,NULL,code,NULL,NULL,&options,NULL);
}
#endif
/* For the eventual monitor; a success means reset was requested, not observed. */
NTSTATUS A04ThermalRequestStop(WDFDEVICE dev)
{ THERMAL_CONTEXT *c=ThermalContext(dev); return c->watchdog ? WatchdogRequest(c,A04_TOPRGU_THERMAL_HALT) : STATUS_DEVICE_NOT_READY; }
/* LIMIT fails safe to reset until a validated frequency-limiting backend exists.
 * This callback is not started by the calibration-only diagnostic. */
int A04ThermalApply(void *context,enum a04_thermal_action action)
{
 if(action==A04_TEMP_NORMAL) return 0;
 if(action!=A04_TEMP_LIMIT && action!=A04_TEMP_HALT) return -1;
 return NT_SUCCESS(A04ThermalRequestStop((WDFDEVICE)context)) ? 0 : -1;
}
NTSTATUS ThermalD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE prior)
{
 THERMAL_CONTEXT *c=ThermalContext(dev); BOOT_CAL v; ULONG size=sizeof(v),attributes=0;
 UNICODE_STRING name=RTL_CONSTANT_STRING(THERMAL_VAR_NAME); GUID guid=calGuid; NTSTATUS status;
 UNREFERENCED_PARAMETER(prior);c->calibrated=FALSE; a04_owner_revoke(&c->ownership);
 c->ownership.resources=c->pnp_resources ? 1 : 0; RtlZeroMemory(&v,sizeof(v));
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
#if A04_THERMAL_LEASE_DIAG
 {
  DECLARE_CONST_UNICODE_STRING(path,A04_TOUCH_THERMAL_DEVICE);
  WDF_IO_TARGET_OPEN_PARAMS open;
  WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,GENERIC_READ|GENERIC_WRITE);
  status=WdfIoTargetOpen(c->touch,&open);
  if(!NT_SUCCESS(status)) { WdfIoTargetClose(c->watchdog); return status; }
  status=TouchRequest(c,A04_TOUCH_THERMAL_CLAIM);
  if(!NT_SUCCESS(status)) { WdfIoTargetClose(c->touch); WdfIoTargetClose(c->watchdog); return status; }
  c->lease_owned=TRUE; c->ownership.touch_lease=1;
  DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THERM touch lease acquired; no clock or reset writes\n");
 }
#endif
 /* Firmware/EL3 handoff has not been proven: the monitor remains blocked.
  * Never infer this flag from ACPI resource allocation or a touch lease. */
 c->ownership.firmware_handoff=0;
 c->calibrated=TRUE;
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THERM boot calibration accepted; measurement disabled\n");
 return STATUS_SUCCESS;
}
NTSTATUS ThermalExit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE next)
{
 THERMAL_CONTEXT *c=ThermalContext(dev); UNREFERENCED_PARAMETER(next);
#if A04_THERMAL_LEASE_DIAG
 if(c->lease_owned) { (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE); c->lease_owned=FALSE; }
 WdfIoTargetClose(c->touch);
#endif
 WdfIoTargetClose(c->watchdog); a04_owner_revoke(&c->ownership); RtlSecureZeroMemory(&c->cal,sizeof(c->cal)); c->calibrated=FALSE;return STATUS_SUCCESS;
}
NTSTATUS ThermalAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS p; WDF_OBJECT_ATTRIBUTES a;WDFDEVICE device; NTSTATUS status;
 UNREFERENCED_PARAMETER(driver);WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);
 p.EvtDevicePrepareHardware=ThermalPrepare;p.EvtDeviceD0Entry=ThermalD0;p.EvtDeviceD0Exit=ThermalExit;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,THERMAL_CONTEXT);
 status=WdfDeviceCreate(&init,&a,&device); if(!NT_SUCCESS(status)) return status;
 WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ParentObject=device;
 status=WdfIoTargetCreate(device,&a,&ThermalContext(device)->watchdog);
 if(!NT_SUCCESS(status)) return status;
#if A04_THERMAL_LEASE_DIAG
 return WdfIoTargetCreate(device,&a,&ThermalContext(device)->touch);
#else
 return STATUS_SUCCESS;
#endif
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{ WDF_DRIVER_CONFIG c;WDF_DRIVER_CONFIG_INIT(&c,ThermalAdd);return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE); }
