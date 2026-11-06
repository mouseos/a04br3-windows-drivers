/* SPDX-License-Identifier: GPL-2.0-only
 * Calibration transport diagnostic only. Does not enable MMIO, clocks,
 * temperature polling, power control or claim to provide thermal protection. */
#include <ntddk.h>
#include <wdf.h>
#include "thermal.h"
#include "manager.h"
#include "ownership.h"
#include "resources.h"
#ifndef A04_THERMAL_SHARED_PARENT
#define A04_THERMAL_SHARED_PARENT 0
#endif
#ifndef A04_THERMAL_LEASE_DIAG
#define A04_THERMAL_LEASE_DIAG 0
#endif
#ifndef A04_THERMAL_ONESHOT_TEST
#define A04_THERMAL_ONESHOT_TEST 0
#endif
#ifndef A04_THERMAL_MONITOR_TEST
#define A04_THERMAL_MONITOR_TEST 0
#endif
#ifndef A04_THERMAL_CODEC_ADC_DIAGNOSTIC
#define A04_THERMAL_CODEC_ADC_DIAGNOSTIC 0
#endif
#if A04_THERMAL_CODEC_ADC_DIAGNOSTIC && (!A04_THERMAL_SHARED_PARENT || !A04_THERMAL_ONESHOT_TEST)
#error codec ADC diagnostic requires the shared-parent one-shot path
#endif
#if A04_THERMAL_CODEC_ADC_DIAGNOSTIC
#include "../../bus/mt8167-shared/codec_adc_probe.h"
#include "../../bus/mt8167-shared/codec_adc_session.h"
#include "../../system/mt8167-toprgu/uart_screen.h"
#endif
#if A04_THERMAL_SHARED_PARENT && !A04_THERMAL_MONITOR_TEST
#error shared thermal resources require diagnostic monitor infrastructure
#endif
#if A04_THERMAL_ONESHOT_TEST && !A04_THERMAL_MONITOR_TEST
#error one-shot requires monitor infrastructure
#endif
#if A04_THERMAL_MONITOR_TEST && !A04_THERMAL_LEASE_DIAG
#error monitor test requires touch lease diagnostic
#endif
#if A04_THERMAL_MONITOR_TEST
#include "monitor.h"
#if A04_THERMAL_ONESHOT_TEST
#include "oneshot.h"
#endif
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
#if A04_THERMAL_MONITOR_TEST
 A04_TOUCH_THERMAL_GUARD touch_guard;
#endif
#endif
#if A04_THERMAL_MONITOR_TEST
 WDFDEVICE device; WDFTIMER timer; volatile UCHAR *mapped[3];
 struct a04_monitor monitor; struct a04_thermal_hw hw; BOOLEAN hw_ready;
 WDFSPINLOCK readout_lock; struct a04_thermal_readout_state readout;
 BOOLEAN reset_asserted;
#if A04_THERMAL_CODEC_ADC_DIAGNOSTIC
 struct a04_codec_adc_probe codec_adc; BOOLEAN codec_route_verified;
#endif
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
static const GUID thermalHandoffGuid={0xb2dd7543,0x9b8a,0x4580,{0xb8,0x22,0x14,0x35,0x09,0xa0,0x47,0xc1}};
typedef struct { ULONG version,bytes,observation,ownership; } THERMAL_HANDOFF_RECORD;
static const GUID calGuid={0x92d56a32,0xc847,0x4fdc,{0x83,0x62,0xf2,0x77,0xc7,0x10,0xa0,0x43}};
static int read_cal(void *p,unsigned index,uint32_t *value)
{ const BOOT_CAL *v=p; if(index==60)*value=v->d60; else if(index==61)*value=v->d61; else if(index==62)*value=v->d62; else return -1; return 0; }
NTSTATUS ThermalPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 static const ULONGLONG addresses[]={0x1100d000,0x11003000,0x10018000,0x10009000};
 ULONG i;struct a04_thermal_resource resources[4]; THERMAL_CONTEXT *c=ThermalContext(dev);
 c->pnp_resources=FALSE;
#if A04_THERMAL_SHARED_PARENT
 if(WdfCmResourceListGetCount(raw)!=2 || WdfCmResourceListGetCount(translated)!=2)return STATUS_DEVICE_CONFIGURATION_ERROR;
#else
 if(WdfCmResourceListGetCount(raw)!=4 || WdfCmResourceListGetCount(translated)!=4)return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
 RtlZeroMemory(resources,sizeof(resources));
 for(i=0;i<WdfCmResourceListGetCount(translated);i++) {
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
#if A04_THERMAL_SHARED_PARENT
 if(a04_thermal_resources_validate_shared(resources,2))return STATUS_DEVICE_CONFIGURATION_ERROR;
#else
 if(a04_thermal_resources_validate(resources,4))return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
 c->pnp_resources=TRUE;
#if A04_THERMAL_MONITOR_TEST
 for(i=0;i<(A04_THERMAL_SHARED_PARENT?2u:3u);i++) {
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
#if A04_THERMAL_MONITOR_TEST
static NTSTATUS TouchGuardGet(THERMAL_CONTEXT *c)
{
 WDF_MEMORY_DESCRIPTOR output;WDF_REQUEST_SEND_OPTIONS options;ULONG_PTR got=0;NTSTATUS status;
 RtlZeroMemory(&c->touch_guard,sizeof(c->touch_guard));
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&output,&c->touch_guard,sizeof(c->touch_guard));
 WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
 WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(1));
 status=WdfIoTargetSendInternalIoctlSynchronously(c->touch,NULL,A04_TOUCH_THERMAL_GET_GUARD,NULL,&output,&options,&got);
 if(!NT_SUCCESS(status))return status;
 if(got!=sizeof(c->touch_guard)||c->touch_guard.size!=sizeof(c->touch_guard)||c->touch_guard.version!=1 ||
    !c->touch_guard.context || !c->touch_guard.owner || !c->touch_guard.generation || !c->touch_guard.Begin || !c->touch_guard.End || !c->touch_guard.Release ||
    (A04_THERMAL_SHARED_PARENT && (!c->touch_guard.ReadAnalog || !c->touch_guard.WriteAnalog))) {
  if(c->touch_guard.context && c->touch_guard.Release)c->touch_guard.Release(c->touch_guard.context);
  RtlZeroMemory(&c->touch_guard,sizeof(c->touch_guard));return STATUS_DEVICE_CONFIGURATION_ERROR;
 }
 return STATUS_SUCCESS;
}
static void TouchGuardPut(THERMAL_CONTEXT *c)
{ if(c->touch_guard.Release)c->touch_guard.Release(c->touch_guard.context);RtlZeroMemory(&c->touch_guard,sizeof(c->touch_guard)); }
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
static int HwQualified(void *p) { THERMAL_CONTEXT *c=p; return c->ownership.active && c->lease_owned && c->touch_guard.Begin && c->touch_guard.End && c->mapped[0] && c->mapped[1] && (A04_THERMAL_SHARED_PARENT || c->mapped[2]); }
static int HwReady(void *p) { return ((THERMAL_CONTEXT *)p)->hw_ready; }
static int HwClocks(void *p) { return NT_SUCCESS(TouchRequest(p,A04_TOUCH_THERMAL_CLOCK))?0:-1; }
static volatile ULONG *HwAddress(THERMAL_CONTEXT *c,uint32_t addr) {
 static const uint32_t bases[]={0x1100d000u,0x11003000u,0x10018000u}; unsigned i;
 for(i=0;i<3;i++) if(addr>=bases[i] && addr-bases[i]<=0xffcu && !(addr&3u)) return (volatile ULONG *)(c->mapped[i]+addr-bases[i]);
 return NULL;
}
static int HwRead(void *p,uint32_t addr,uint32_t *value) { THERMAL_CONTEXT *c=p;volatile ULONG *r;
 if(!HwQualified(c) || !value)return -1;
#if A04_THERMAL_SHARED_PARENT
 if(addr==0x10018604u) {
  NTSTATUS status;if(!c->touch_guard.Begin(c->touch_guard.context,c->touch_guard.owner,c->touch_guard.generation))return -1;
  status=c->touch_guard.ReadAnalog(c->touch_guard.context,value);c->touch_guard.End(c->touch_guard.context);
  return NT_SUCCESS(status)?0:-1;
 }
#endif
 if(!(r=HwAddress(c,addr)))return -1;
 if(!c->touch_guard.Begin(c->touch_guard.context,c->touch_guard.owner,c->touch_guard.generation))return -1;
 *value=READ_REGISTER_ULONG(r);c->touch_guard.End(c->touch_guard.context);return 0; }
static int HwWrite(void *p,uint32_t addr,uint32_t value) { THERMAL_CONTEXT *c=p;volatile ULONG *r;
 if(!HwQualified(c)) return -1;
 if(addr==0x10001120u && value==1) { if(!NT_SUCCESS(TouchRequest(c,A04_TOUCH_THERMAL_RESET)))return -1;c->reset_asserted=TRUE;return 0; }
 if(addr==0x10001124u && value==1 && c->reset_asserted) { c->reset_asserted=FALSE;return 0; }
#if A04_THERMAL_SHARED_PARENT
 if(addr==0x10018604u) {
  NTSTATUS status;if(!c->touch_guard.Begin(c->touch_guard.context,c->touch_guard.owner,c->touch_guard.generation))return -1;
  status=c->touch_guard.WriteAnalog(c->touch_guard.context,value);c->touch_guard.End(c->touch_guard.context);
  return NT_SUCCESS(status)?0:-1;
 }
#endif
 r=HwAddress(c,addr);if(!r)return -1;
 if(!c->touch_guard.Begin(c->touch_guard.context,c->touch_guard.owner,c->touch_guard.generation))return -1;
 WRITE_REGISTER_ULONG(r,value);c->touch_guard.End(c->touch_guard.context);return 0; }
static int HwDelay(void *p,unsigned us) { UNREFERENCED_PARAMETER(p);if(us>1000)return -1;KeStallExecutionProcessor(us);return 0; }
#if A04_THERMAL_CODEC_ADC_DIAGNOSTIC
/* The shared audio owner has no provider that verifies both the headphone
 * analog route and the downstream physical mute. Never infer either from
 * a thermal lease or from the Class D gain. This independent prerequisite
 * must be implemented before actual sampling can be admitted. */
static int CodecAdcQualified(void *p) {
 THERMAL_CONTEXT *c=p;
 return HwQualified(c) && c->hw_ready && c->codec_route_verified;
}
/* There is not yet an evidence-backed route/mute provider. */
static int CodecAdcPreflight(void *p) {
 THERMAL_CONTEXT *c=p;
 c->codec_route_verified=FALSE;
 return -1;
}
static int CodecAdcClock(void *p) {
 THERMAL_CONTEXT *c=p;
 if(!NT_SUCCESS(TouchRequest(c,A04_TOUCH_THERMAL_CLOCK)))return -1;
 c->hw_ready=TRUE;return 0;
}
static int CodecAdcRead(void *p,uint32_t offset,uint16_t *value) {
 THERMAL_CONTEXT *c=p;
 if(!HwQualified(c) || !value || !c->mapped[1] || offset>0xffeu || (offset&1u) ||
    !c->touch_guard.Begin(c->touch_guard.context,c->touch_guard.owner,c->touch_guard.generation))return -1;
 *value=READ_REGISTER_USHORT((volatile USHORT *)(c->mapped[1]+offset));
 c->touch_guard.End(c->touch_guard.context);return 0;
}
static int CodecAdcWrite(void *p,uint32_t offset,uint16_t value) {
 THERMAL_CONTEXT *c=p;
 if(!HwQualified(c) || !c->mapped[1] || offset!=4 ||
    !c->touch_guard.Begin(c->touch_guard.context,c->touch_guard.owner,c->touch_guard.generation))return -1;
 WRITE_REGISTER_USHORT((volatile USHORT *)(c->mapped[1]+offset),value);
 c->touch_guard.End(c->touch_guard.context);return 0;
}
static NTSTATUS CodecAdcUart(THERMAL_CONTEXT *c,const char *message,ULONG length) {
 WDF_MEMORY_DESCRIPTOR input;WDF_REQUEST_SEND_OPTIONS options;
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&input,(PVOID)message,length);
 WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
 WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(1));
 /* Delivery is asynchronous after the IOCTL completes; failed send or
  * missing UART bytes is not evidence of successful transmission. */
 return WdfIoTargetSendIoctlSynchronously(c->watchdog,NULL,A04_UART_SCREEN_SEND,&input,NULL,&options,NULL);
}
static int CodecAdcCapture(THERMAL_CONTEXT *c,struct a04_codec_adc_samples *samples) {
 struct a04_codec_adc_io io={c,CodecAdcQualified,CodecAdcRead,CodecAdcWrite,HwDelay};
 return a04_codec_adc_capture(&c->codec_adc,&io,7,samples);
}
static int CodecAdcCaptureCallback(void *p,struct a04_codec_adc_samples *samples) {
 return CodecAdcCapture((THERMAL_CONTEXT *)p,samples);
}
/* Clock and all mappings remain held. Stopped control bits alone are not a
 * general fabric drain proof; these checks only qualify this ADC diagnostic's
 * shared thermal lease for its existing quiescent contract. */
static int CodecAdcQuiesce(void *p) {
 THERMAL_CONTEXT *c=p;uint16_t trigger,busy;uint32_t enabled,ahb;
 if(!CodecAdcQualified(c) || CodecAdcRead(c,4,&trigger) || (trigger&(1u<<7)) ||
    CodecAdcRead(c,0x10,&busy) || (busy&1u) ||
    HwRead(c,0x1100d000u,&enabled) || enabled ||
    HwRead(c,0x1100d418u,&ahb) || (ahb>>16))return -1;
 return NT_SUCCESS(TouchRequest(c,A04_TOUCH_THERMAL_QUIESCENT))?0:-1;
}
static int CodecAdcSendSamples(THERMAL_CONTEXT *c,const struct a04_codec_adc_samples *samples) {
 static const char hex[]="0123456789ABCDEF";
 unsigned part,i,n,retry;ULONG tag=(ULONG)KeQueryInterruptTime();char line[80];NTSTATUS status;LARGE_INTEGER delay;
 delay.QuadPart=-100000; /* 10 ms; UART queue is intentionally bounded. */
 for(part=0;part<2;part++) {
  const char prefix[]="A04ADC HPL ";n=0;
  for(i=0;i<sizeof(prefix)-1;i++)line[n++]=prefix[i];
  for(i=0;i<8;i++)line[n++]=hex[(tag>>(28-4*i))&15];
  line[n++]=' ';line[n++]=(char)('0'+part);line[n++]=' ';
  for(i=0;i<11;i++) {
   uint16_t raw=samples->raw[part*11+i];
   line[n++]=hex[(raw>>8)&15];line[n++]=hex[(raw>>4)&15];line[n++]=hex[raw&15];line[n++]=' ';
  }
  line[n++]='\n';status=STATUS_DEVICE_BUSY;
  for(retry=0;retry<200 && status==STATUS_DEVICE_BUSY;retry++) {
   status=CodecAdcUart(c,line,n);
   if(status==STATUS_DEVICE_BUSY && KeDelayExecutionThread(KernelMode,FALSE,&delay)!=STATUS_SUCCESS)return -1;
  }
  if(!NT_SUCCESS(status))return -1;
 }
 return 0; /* queued, not proof that UART transmitted either line */
}
#endif
#if A04_THERMAL_ONESHOT_TEST
static uint64_t ThermalOneNow(void *p) { UNREFERENCED_PARAMETER(p);return KeQueryInterruptTime()/10000u; }
static int ThermalOneWait(void *p,unsigned ms) {
 LARGE_INTEGER interval;UNREFERENCED_PARAMETER(p);if(!ms || ms>10)return -1;
 interval.QuadPart=-(LONGLONG)ms*10000;return NT_SUCCESS(KeDelayExecutionThread(KernelMode,FALSE,&interval))?0:-1;
}
static int ThermalOneReady(void *p,int ready) { ((THERMAL_CONTEXT *)p)->hw_ready=ready?TRUE:FALSE;return 0; }
static int ThermalOneReport(void *p,const struct a04_thermal_sample *sample) {
 THERMAL_CONTEXT *c=p;int result;
 WdfSpinLockAcquire(c->readout_lock);result=a04_readout_publish(&c->readout,sample);WdfSpinLockRelease(c->readout_lock);
 return result;
}
#endif
NTSTATUS ThermalRelease(WDFDEVICE dev,WDFCMRESLIST translated) {
 THERMAL_CONTEXT *c=ThermalContext(dev);unsigned i;UNREFERENCED_PARAMETER(translated);
 c->pnp_resources=FALSE; ThermalInvalidate(c); a04_owner_revoke(&c->ownership);TouchGuardPut(c);
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
 {
  UNICODE_STRING handoffName=RTL_CONSTANT_STRING(L"A04BR3ThermalHandoff");
  GUID handoffGuid=thermalHandoffGuid;THERMAL_HANDOFF_RECORD handoff;
  ULONG handoffSize=sizeof(handoff),handoffAttributes=0;NTSTATUS observed;
  RtlZeroMemory(&handoff,sizeof(handoff));
  observed=ExGetFirmwareEnvironmentVariable(&handoffName,&handoffGuid,&handoff,&handoffSize,&handoffAttributes);
  if(NT_SUCCESS(observed) && handoffSize==sizeof(handoff) && handoffAttributes==6 &&
     handoff.version==1 && handoff.bytes==sizeof(handoff) && handoff.observation==1 && handoff.ownership==0)
   DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THERM UEFI observation present; firmware and EL3 ownership unverified\n");
  else DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THERM UEFI handoff observation unavailable or invalid\n");
  /* Observation is not a firmware or EL3 release and never opens the gate. */
 }
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
  if(!NT_SUCCESS(status)) {
#if A04_THERMAL_CODEC_ADC_DIAGNOSTIC
   CodecAdcUart(c,"A04ADC REJECT LEASE\n",20);
#endif
   WdfIoTargetClose(c->touch); WdfIoTargetClose(c->watchdog); return status; }
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
 status=TouchGuardGet(c);
 if(!NT_SUCCESS(status)) {
  if(c->lease_owned) { (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);c->lease_owned=FALSE; }
  WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);return status;
 }
#endif
#if A04_THERMAL_MONITOR_TEST
 /* No verified firmware/EL3 release protocol exists. Buildable, but cannot
  * touch clocks, AUXADC or controller while this qualification is false. */
 if(a04_owner_activate(&c->ownership)) {
   TouchGuardPut(c);
   if(c->lease_owned) { (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);c->lease_owned=FALSE; }
#if A04_THERMAL_CODEC_ADC_DIAGNOSTIC
   CodecAdcUart(c,"A04ADC REJECT HANDOFF\n",22);
#endif
   WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);
   return STATUS_DEVICE_NOT_READY;
 }
#if A04_THERMAL_CODEC_ADC_DIAGNOSTIC
 {
  struct a04_codec_adc_samples samples;enum a04_codec_adc_session_result result;
  struct a04_codec_adc_session_io io={c,CodecAdcPreflight,CodecAdcClock,CodecAdcCaptureCallback,CodecAdcQuiesce};
  result=a04_codec_adc_session(&io,&samples);
  if(result!=A04_ADC_COMPLETE) {
   if(result==A04_ADC_UNCERTAIN) { a04_owner_quarantine(&c->ownership);KeBugCheckEx(0xE2,0x41303454,7,0,0);return STATUS_DEVICE_HARDWARE_ERROR; }
   TouchGuardPut(c);
   status=TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);
   if(!NT_SUCCESS(status)) { a04_owner_quarantine(&c->ownership);return status; }
   c->lease_owned=FALSE;a04_owner_revoke(&c->ownership);
   CodecAdcUart(c,"A04ADC REJECT ROUTE_OR_MUTE\n",28);
   WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);return STATUS_DEVICE_NOT_READY;
  }
  /* Success is deliberately unreachable until a real route/mute provider
   * replaces CodecAdcQualified. Values are not AU_LOL calibration. */
  if(CodecAdcSendSamples(c,&samples))
   DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04ADC UART queue incomplete\n");
  TouchGuardPut(c);
  status=TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);
  if(!NT_SUCCESS(status)) { a04_owner_quarantine(&c->ownership);return status; }
  c->lease_owned=FALSE;c->hw_ready=FALSE;c->codec_route_verified=FALSE;
  a04_owner_revoke(&c->ownership);WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);
  c->calibrated=TRUE;return STATUS_SUCCESS;
 }
#endif
#if A04_THERMAL_ONESHOT_TEST
 {
  struct a04_oneshot_io io;enum a04_oneshot_result measured;
  c->hw.ctx=c;c->hw.qualified=HwQualified;c->hw.clocks_on=HwClocks;c->hw.initialized=HwReady;
  c->hw.read32=HwRead;c->hw.write32=HwWrite;c->hw.delay_us=HwDelay;c->hw_ready=FALSE;c->reset_asserted=FALSE;
  io.hw=&c->hw;io.context=c;io.now_ms=ThermalOneNow;io.wait_ms=ThermalOneWait;
  io.set_ready=ThermalOneReady;io.report=ThermalOneReport;
  measured=a04_thermal_oneshot(&io,&c->cal);
  if(measured==A04_ONESHOT_SHUTDOWN_FAILED) {
   ThermalInvalidate(c);a04_owner_quarantine(&c->ownership);
   /* No safe lease return is known after partial MMIO. Stop immediately;
    * bugcheck is not proof that reset or physical power isolation occurred. */
   KeBugCheckEx(0xE2,0x41303454,6,0,0);return STATUS_DEVICE_HARDWARE_ERROR;
  }
  if(measured!=A04_ONESHOT_OK)ThermalInvalidate(c);
#if A04_THERMAL_SHARED_PARENT
  /* oneshot returns here only before initialization or after successful
   * quiesce; a failed quiesce bugchecks above rather than releasing. */
  if(!NT_SUCCESS(TouchRequest(c,A04_TOUCH_THERMAL_QUIESCENT))) {
   ThermalInvalidate(c);a04_owner_quarantine(&c->ownership);return STATUS_DEVICE_HARDWARE_ERROR;
  }
#endif
  TouchGuardPut(c);
  status=TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);
  if(!NT_SUCCESS(status)) { ThermalInvalidate(c);a04_owner_quarantine(&c->ownership);return status; }
  c->lease_owned=FALSE;a04_owner_revoke(&c->ownership);
  if(measured!=A04_ONESHOT_OK) { WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);return STATUS_DEVICE_HARDWARE_ERROR; }
  c->calibrated=TRUE;return STATUS_SUCCESS;
 }
#else
 {
  struct a04_thermal_policy policy={75000,65000,85000,1000,0};
  if(!NT_SUCCESS(WatchdogRequest(c,A04_TOPRGU_THERMAL_ARM))) {
   a04_owner_revoke(&c->ownership);TouchGuardPut(c); (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);c->lease_owned=FALSE;
   WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);return STATUS_DEVICE_NOT_READY;
  }
  c->hw.ctx=c;c->hw.qualified=HwQualified;c->hw.clocks_on=HwClocks;c->hw.initialized=HwReady;
  c->hw.read32=HwRead;c->hw.write32=HwWrite;c->hw.delay_us=HwDelay;c->hw_ready=FALSE;c->reset_asserted=FALSE;
  int monitor_status=a04_monitor_start(&c->monitor,&c->hw,&c->cal,&policy,dev,A04ThermalApply,KeQueryInterruptTime()/10000u,2000);
  if(monitor_status) {
   a04_owner_quarantine(&c->ownership);
   if(monitor_status==-2) KeBugCheckEx(0xE2,0x41303454,2,0,0);
   a04_owner_revoke(&c->ownership);TouchGuardPut(c); (VOID)TouchRequest(c,A04_TOUCH_THERMAL_RELEASE);c->lease_owned=FALSE;
   WdfIoTargetClose(c->touch);WdfIoTargetClose(c->watchdog);return STATUS_DEVICE_HARDWARE_ERROR;
  }
  c->hw_ready=TRUE;WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(250));
 }
#endif /* one-shot or monitor */
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
#if A04_THERMAL_MONITOR_TEST
 TouchGuardPut(c);
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
