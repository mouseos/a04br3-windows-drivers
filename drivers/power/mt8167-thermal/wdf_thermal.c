/* SPDX-License-Identifier: GPL-2.0-only
 * Calibration transport diagnostic only. Does not enable MMIO, clocks,
 * temperature polling, power control or claim to provide thermal protection. */
#include <ntddk.h>
#include <wdf.h>
#include "thermal.h"
#include "manager.h"
#include "ownership.h"
#include "resources.h"
#ifndef A04_THERMAL_LEASE_DIAG
#define A04_THERMAL_LEASE_DIAG 0
#endif
#ifndef A04_THERMAL_MONITOR_TEST
#define A04_THERMAL_MONITOR_TEST 0
#endif
#if A04_THERMAL_MONITOR_TEST && !A04_THERMAL_LEASE_DIAG
#error monitor test requires touch lease diagnostic
#endif
#if A04_THERMAL_MONITOR_TEST
#include "monitor.h"
#include "readout.h"
#include "readout_ioctl.h"
EVT_WDF_TIMER ThermalMonitorTimer;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL ThermalReadoutIoctl;
#endif
#include "../../system/mt8167-toprgu/thermal_stop.h"
#if A04_THERMAL_LEASE_DIAG
#include "../../input/bu21025/thermal_broker.h"
#endif
#define THERMAL_VAR_NAME L"A04BR3ThermalCalibration"
typedef struct { ULONG version, d60, d61, d62; } BOOT_CAL;
typedef struct { struct a04_thermal_cal cal; struct a04_thermal_owner ownership; BOOLEAN calibrated, pnp_resources; WDFIOTARGET watchdog;
#if A04_THERMAL_LEASE_DIAG
 WDFIOTARGET touch; BOOLEAN lease_owned;
#endif
#if A04_THERMAL_MONITOR_TEST
 WDFDEVICE device; WDFTIMER timer; volatile UCHAR *mapped[3];
 struct a04_monitor monitor; struct a04_thermal_hw hw; BOOLEAN hw_ready;
 WDFSPINLOCK readout_lock; struct a04_thermal_readout_state readout;
 BOOLEAN reset_asserted;
#endif
} THERMAL_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(THERMAL_CONTEXT,ThermalContext)
EVT_WDF_DRIVER_DEVICE_ADD ThermalAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE ThermalPrepare;
#if A04_THERMAL_MONITOR_TEST
EVT_WDF_DEVICE_RELEASE_HARDWARE ThermalRelease;
#endif
EVT_WDF_DEVICE_D0_ENTRY ThermalD0;
EVT_WDF_DEVICE_D0_EXIT ThermalExit;
static const GUID calGuid={0x92d56a32,0xc847,0x4fdc,{0x83,0x62,0xf2,0x77,0xc7,0x10,0xa0,0x43}};
static int read_cal(void *p,unsigned index,uint32_t *value)
{ const BOOT_CAL *v=p; if(index==60)*value=v->d60; else if(index==61)*value=v->d61; else if(index==62)*value=v->d62; else return -1; return 0; }
NTSTATUS ThermalPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 static const ULONGLONG addresses[]={0x1100d000,0x11003000,0x10018000,0x10009000};
 ULONG i;struct a04_thermal_resource resources[4]; THERMAL_CONTEXT *c=ThermalContext(dev);
 c->pnp_resources=FALSE;
 if(WdfCmResourceListGetCount(raw)!=4 || WdfCmResourceListGetCount(translated)!=4)
  return STATUS_DEVICE_CONFIGURATION_ERROR;
 RtlZeroMemory(resources,sizeof(resources));
 for(i=0;i<4;i++) {
  PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
  PCM_PARTIAL_RESOURCE_DESCRIPTOR r=WdfCmResourceListGetDescriptor(raw,i);
  if(!d || !r || d->Type!=CmResourceTypeMemory || r->Type!=CmResourceTypeMemory)
   return STATUS_DEVICE_CONFIGURATION_ERROR;
  resources[i].raw_memory=resources[i].memory=1;
  resources[i].raw_exclusive=r->ShareDisposition==CmResourceShareDeviceExclusive;
  resources[i].exclusive=d->ShareDisposition==CmResourceShareDeviceExclusive;
  resources[i].raw=(uint64_t)r->u.Memory.Start.QuadPart;
  resources[i].translated=(uint64_t)d->u.Memory.Start.QuadPart;
  resources[i].raw_size=r->u.Memory.Length;resources[i].size=d->u.Memory.Length;
 }
 if(a04_thermal_resources_validate(resources,4))return STATUS_DEVICE_CONFIGURATION_ERROR;
 c->pnp_resources=TRUE;
#if A04_THERMAL_MONITOR_TEST
 for(i=0;i<3;i++) {
  PHYSICAL_ADDRESS a; a.QuadPart=addresses[i]; c->mapped[i]=MmMapIoSpace(a,0x1000,MmNonCached);
  if(!c->mapped[i]) { while(i) { --i;MmUnmapIoSpace((PVOID)c->mapped[i],0x1000);c->mapped[i]=NULL; } c->pnp_resources=FALSE; return STATUS_INSUFFICIENT_RESOURCES; }
 }
#endif
 return STATUS_SUCCESS;
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
#if A04_THERMAL_MONITOR_TEST
/* Publication and retrieval share one nonpaged lock; no fabricated or stale
 * reading is returned while startup, failure, stop or power exit is pending. */
static void ThermalInvalidate(THERMAL_CONTEXT *c) {
 WdfSpinLockAcquire(c->readout_lock);a04_readout_invalidate(&c->readout);WdfSpinLockRelease(c->readout_lock);
}
VOID ThermalReadoutIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outputSize,size_t inputSize,ULONG code) {
 THERMAL_CONTEXT *c=ThermalContext(WdfIoQueueGetDevice(queue));
 struct a04_thermal_readout_v1 value,*output;size_t actual=0;NTSTATUS status;
 UNREFERENCED_PARAMETER(inputSize);
 if(code!=A04_THERMAL_GET_READING) { WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return; }
 if(outputSize<sizeof(value)) { WdfRequestComplete(request,STATUS_BUFFER_TOO_SMALL);return; }
 status=WdfRequestRetrieveOutputBuffer(request,sizeof(value),(PVOID *)&output,&actual);
 if(!NT_SUCCESS(status)) { WdfRequestComplete(request,status);return; }
 WdfSpinLockAcquire(c->readout_lock);
 if(a04_readout_get(&c->readout,KeQueryInterruptTime()/10000u,1000,&value)) status=STATUS_DEVICE_NOT_READY;
 else { *output=value;status=STATUS_SUCCESS; }
 WdfSpinLockRelease(c->readout_lock);
 WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status)?sizeof(value):0);
}
static int HwQualified(void *p) { THERMAL_CONTEXT *c=p; return c->ownership.active && c->lease_owned && c->mapped[0] && c->mapped[1] && c->mapped[2]; }
static int HwReady(void *p) { return ((THERMAL_CONTEXT *)p)->hw_ready; }
static int HwClocks(void *p) { return NT_SUCCESS(TouchRequest(p,A04_TOUCH_THERMAL_CLOCK))?0:-1; }
static volatile ULONG *HwAddress(THERMAL_CONTEXT *c,uint32_t addr) {
 static const uint32_t bases[]={0x1100d000u,0x11003000u,0x10018000u}; unsigned i;
 for(i=0;i<3;i++) if(addr>=bases[i] && addr-bases[i]<=0xffcu && !(addr&3u)) return (volatile ULONG *)(c->mapped[i]+addr-bases[i]);
 return NULL;
}
static int HwRead(void *p,uint32_t addr,uint32_t *value) { THERMAL_CONTEXT *c=p;volatile ULONG *r;
 if(!HwQualified(c) || !value || !(r=HwAddress(c,addr))) return -1;*value=READ_REGISTER_ULONG(r);return 0; }
static int HwWrite(void *p,uint32_t addr,uint32_t value) { THERMAL_CONTEXT *c=p;volatile ULONG *r;
 if(!HwQualified(c)) return -1;
 if(addr==0x10001120u && value==1) { if(!NT_SUCCESS(TouchRequest(c,A04_TOUCH_THERMAL_RESET)))return -1;c->reset_asserted=TRUE;return 0; }
 if(addr==0x10001124u && value==1 && c->reset_asserted) { c->reset_asserted=FALSE;return 0; }
 r=HwAddress(c,addr);if(!r)return -1;WRITE_REGISTER_ULONG(r,value);return 0; }
static int HwDelay(void *p,unsigned us) { UNREFERENCED_PARAMETER(p);if(us>1000)return -1;KeStallExecutionProcessor(us);return 0; }
NTSTATUS ThermalRelease(WDFDEVICE dev,WDFCMRESLIST translated) {
 THERMAL_CONTEXT *c=ThermalContext(dev);unsigned i;UNREFERENCED_PARAMETER(translated);
 c->pnp_resources=FALSE; ThermalInvalidate(c); a04_owner_revoke(&c->ownership);
 for(i=0;i<3;i++) if(c->mapped[i]) { MmUnmapIoSpace((PVOID)c->mapped[i],0x1000);c->mapped[i]=NULL; }
 return STATUS_SUCCESS;
}
VOID ThermalMonitorTimer(WDFTIMER timer) {
 WDFDEVICE dev=(WDFDEVICE)WdfTimerGetParentObject(timer);THERMAL_CONTEXT *c=ThermalContext(dev);
 struct a04_thermal_sample sample;int result;
 if(!c->ownership.active){ThermalInvalidate(c);return;}
 if(!NT_SUCCESS(TouchRequest(c,A04_TOUCH_THERMAL_QUERY))) {
  ThermalInvalidate(c); a04_owner_quarantine(&c->ownership);
  if(!NT_SUCCESS(A04ThermalRequestStop(dev))) KeBugCheckEx(0xE2,0x41303454,1,0,0);
  return;
 }
 result=a04_monitor_poll(&c->monitor,KeQueryInterruptTime()/10000u,&sample);
 if(result==-2) { ThermalInvalidate(c); a04_owner_quarantine(&c->ownership);KeBugCheckEx(0xE2,0x41303454,0,0,0);return; }
 if(result<0) { ThermalInvalidate(c); a04_owner_quarantine(&c->ownership);return; }
 if(result==1 && !NT_SUCCESS(WatchdogRequest(c,A04_TOPRGU_THERMAL_PING))) {
  ThermalInvalidate(c); a04_owner_quarantine(&c->ownership);
  if(!NT_SUCCESS(A04ThermalRequestStop(dev))) KeBugCheckEx(0xE2,0x41303454,3,0,0);
  return;
 }
 if(result==1) {
  int publish;
  WdfSpinLockAcquire(c->readout_lock);publish=a04_readout_publish(&c->readout,&sample);WdfSpinLockRelease(c->readout_lock);
  if(publish) { a04_owner_quarantine(&c->ownership);if(!NT_SUCCESS(A04ThermalRequestStop(dev))) KeBugCheckEx(0xE2,0x41303454,5,0,0);return; }
 }
 WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(250));
}
#endif
NTSTATUS ThermalD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE prior)
{
 THERMAL_CONTEXT *c=ThermalContext(dev); BOOT_CAL v; ULONG size=sizeof(v),attributes=0;
 UNICODE_STRING name=RTL_CONSTANT_STRING(THERMAL_VAR_NAME); GUID guid=calGuid; NTSTATUS status;
 UNREFERENCED_PARAMETER(prior);c->calibrated=FALSE;
#if A04_THERMAL_MONITOR_TEST
 ThermalInvalidate(c);
#endif
 a04_owner_revoke(&c->ownership);
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
#if !A04_THERMAL_MONITOR_TEST
  /* A calibration-only diagnostic must not retain the shared lease for D0. */
  status=TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);
  c->lease_owned=FALSE; c->ownership.touch_lease=0;
  WdfIoTargetClose(c->touch);
  if(!NT_SUCCESS(status)) { WdfIoTargetClose(c->watchdog); return status; }
  DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THERM touch lease released\n");
#endif
 }
#endif
 /* Firmware/EL3 handoff has not been proven: the monitor remains blocked.
  * Never infer this flag from ACPI resource allocation or a touch lease. */
 c->ownership.firmware_handoff=0;
#if A04_THERMAL_MONITOR_TEST
 /* No verified firmware/EL3 release protocol exists. Buildable, but cannot
  * touch clocks, AUXADC or controller while this qualification is false. */
 if(a04_owner_activate(&c->ownership)) {
   if(c->lease_owned) { (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);c->lease_owned=FALSE; }
   WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);
   return STATUS_DEVICE_NOT_READY;
 }
 {
  struct a04_thermal_policy policy={75000,65000,85000,1000,0};
  if(!NT_SUCCESS(WatchdogRequest(c,A04_TOPRGU_THERMAL_ARM))) {
   a04_owner_revoke(&c->ownership); (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);c->lease_owned=FALSE;
   WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);return STATUS_DEVICE_NOT_READY;
  }
  c->hw.ctx=c;c->hw.qualified=HwQualified;c->hw.clocks_on=HwClocks;c->hw.initialized=HwReady;
  c->hw.read32=HwRead;c->hw.write32=HwWrite;c->hw.delay_us=HwDelay;c->hw_ready=FALSE;c->reset_asserted=FALSE;
  int monitor_status=a04_monitor_start(&c->monitor,&c->hw,&c->cal,&policy,dev,A04ThermalApply,KeQueryInterruptTime()/10000u,2000);
  if(monitor_status) {
   a04_owner_quarantine(&c->ownership);
   if(monitor_status==-2) KeBugCheckEx(0xE2,0x41303454,2,0,0);
   a04_owner_revoke(&c->ownership); (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);c->lease_owned=FALSE;
   WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);return STATUS_DEVICE_HARDWARE_ERROR;
  }
  c->hw_ready=TRUE;WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(250));
 }
#endif
 c->calibrated=TRUE;
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THERM boot calibration accepted; measurement disabled\n");
 return STATUS_SUCCESS;
}
NTSTATUS ThermalExit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE next)
{
 THERMAL_CONTEXT *c=ThermalContext(dev); UNREFERENCED_PARAMETER(next);
#if A04_THERMAL_MONITOR_TEST
 WdfTimerStop(c->timer,TRUE); ThermalInvalidate(c); c->hw_ready=FALSE;
 if(c->ownership.active) {
  a04_owner_quarantine(&c->ownership);
  if(!NT_SUCCESS(A04ThermalRequestStop(dev))) KeBugCheckEx(0xE2,0x41303454,4,0,0);
 }
#endif
#if A04_THERMAL_LEASE_DIAG
 if(c->lease_owned) { (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE); c->lease_owned=FALSE; }
#if A04_THERMAL_MONITOR_TEST
 WdfIoTargetClose(c->touch);
#endif
#endif
 WdfIoTargetClose(c->watchdog); a04_owner_revoke(&c->ownership); RtlSecureZeroMemory(&c->cal,sizeof(c->cal)); c->calibrated=FALSE;return STATUS_SUCCESS;
}
NTSTATUS ThermalAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS p; WDF_OBJECT_ATTRIBUTES a;WDFDEVICE device; NTSTATUS status;
 UNREFERENCED_PARAMETER(driver);WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);
 p.EvtDevicePrepareHardware=ThermalPrepare;
#if A04_THERMAL_MONITOR_TEST
 p.EvtDeviceReleaseHardware=ThermalRelease;
#endif
 p.EvtDeviceD0Entry=ThermalD0;p.EvtDeviceD0Exit=ThermalExit;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,THERMAL_CONTEXT);
 status=WdfDeviceCreate(&init,&a,&device); if(!NT_SUCCESS(status)) return status;
 WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ParentObject=device;
 status=WdfIoTargetCreate(device,&a,&ThermalContext(device)->watchdog);
 if(!NT_SUCCESS(status)) return status;
#if A04_THERMAL_LEASE_DIAG
 status=WdfIoTargetCreate(device,&a,&ThermalContext(device)->touch); if(!NT_SUCCESS(status))return status;
#endif
#if A04_THERMAL_MONITOR_TEST
 { WDF_TIMER_CONFIG t; WDF_IO_QUEUE_CONFIG q; WDFQUEUE queue; THERMAL_CONTEXT *c=ThermalContext(device); c->device=device;
   WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=device;
   status=WdfSpinLockCreate(&a,&c->readout_lock);if(!NT_SUCCESS(status))return status;
   WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);q.EvtIoDeviceControl=ThermalReadoutIoctl;
   status=WdfIoQueueCreate(device,&q,WDF_NO_OBJECT_ATTRIBUTES,&queue);if(!NT_SUCCESS(status))return status;
   status=WdfDeviceCreateDeviceInterface(device,&A04ThermalReadoutInterfaceGuid,NULL);if(!NT_SUCCESS(status))return status;
   WDF_TIMER_CONFIG_INIT(&t,ThermalMonitorTimer);t.AutomaticSerialization=FALSE;
   WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=device;a.ExecutionLevel=WdfExecutionLevelPassive;
   return WdfTimerCreate(&t,&a,&c->timer); }
#else
 return STATUS_SUCCESS;
#endif
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{ WDF_DRIVER_CONFIG c;WDF_DRIVER_CONFIG_INIT(&c,ThermalAdd);return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE); }
