/* SPDX-License-Identifier: GPL-2.0-only
 * Explicit native OS one-shot profile; firmware/EL3 release is not asserted.
 * Parent1012 owns all shared clocks/reset/analog. Child owns only thermal/ADC. */
#include <ntddk.h>
#include <wdf.h>
#include "boot_record.h"
#include "resources.h"
#include "oneshot.h"
#include "readout.h"
#include "readout_ioctl.h"
#include "../../bus/mt8167-shared/thermal_native_interface.h"
#include "../../system/mt8167-toprgu/uart_screen.h"
typedef struct {
 WDFDEVICE device;WDFIOTARGET uart;WDFSPINLOCK lock;
 volatile UCHAR *mapped[2];A04_THERMAL_NATIVE_INTERFACE parent;
 ULONG token;BOOLEAN bound,leased,ready,uncertain,reset_acked;volatile LONG stopping;
 struct a04_thermal_hw hw;struct a04_thermal_readout_state reading;
} NATIVE_THERMAL;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NATIVE_THERMAL,Native)
EVT_WDF_DRIVER_DEVICE_ADD NativeAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE NativePrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE NativeRelease;
EVT_WDF_DEVICE_D0_ENTRY NativeD0;
EVT_WDF_DEVICE_D0_EXIT NativeExit;
EVT_WDF_DEVICE_SURPRISE_REMOVAL NativeRemoved;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL NativeIoctl;
static const GUID calGuid={0x92d56a32,0xc847,0x4fdc,{0x83,0x62,0xf2,0x77,0xc7,0x10,0xa0,0x43}};
static void drop_interface(NATIVE_THERMAL *c)
{ if(c->bound && !c->leased){c->parent.Header.InterfaceDereference(c->parent.Header.Context);RtlZeroMemory(&c->parent,sizeof(c->parent));c->bound=FALSE;} }
static void trace(NATIVE_THERMAL *c,const char *s,ULONG n)
{
 WDF_MEMORY_DESCRIPTOR input;WDF_REQUEST_SEND_OPTIONS options;
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"%.*s",(int)n,s);
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&input,(PVOID)s,n);WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
 WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(1));
 (VOID)WdfIoTargetSendIoctlSynchronously(c->uart,NULL,A04_UART_SCREEN_SEND,&input,NULL,&options,NULL);
}
#define TRACE(c,s) trace(c,s,sizeof(s)-1)
static void number(char *s,ULONG *n,int32_t value)
{ char digits[12];unsigned count=0;uint32_t u;if(value<0){s[(*n)++]='-';u=0u-(uint32_t)value;}else u=(uint32_t)value;do{digits[count++]=(char)('0'+u%10);u/=10;}while(u);while(count)s[(*n)++]=digits[--count]; }
static int report(void *p,const struct a04_thermal_sample *s)
{
 NATIVE_THERMAL *c=p;char line[96]="A04THM NATIVE mc=";ULONG n=sizeof("A04THM NATIVE mc=")-1;unsigned i;
 WdfSpinLockAcquire(c->lock);int result=a04_readout_publish(&c->reading,s);WdfSpinLockRelease(c->lock);
 if(result)return -1;
 for(i=0;i<3;i++){number(line,&n,s->mc[i]);line[n++]=i==2?'\n':'/';}
 trace(c,line,n);return 0; /* UART queue acceptance is not transmission proof. */
}
static int qualified(void *p)
{ NATIVE_THERMAL *c=p;return c->bound&&c->leased&&!c->uncertain&&!InterlockedCompareExchange(&c->stopping,0,0)&&c->mapped[0]&&c->mapped[1]; }
static int initialized(void *p){return ((NATIVE_THERMAL *)p)->ready;}
static int clocks(void *p){NATIVE_THERMAL *c=p;return qualified(c)&&NT_SUCCESS(c->parent.Clock(c->parent.Header.Context,c->token))?0:-1;}
static volatile ULONG *address(NATIVE_THERMAL *c,uint32_t a)
{ if(a>=0x1100d000u&&a<=0x1100dffcu&&!(a&3u))return (volatile ULONG *)(c->mapped[0]+(a-0x1100d000u));if(a>=0x11003000u&&a<=0x11003ffcu&&!(a&3u))return (volatile ULONG *)(c->mapped[1]+(a-0x11003000u));return NULL; }
static int read32(void *p,uint32_t a,uint32_t *v)
{
 NATIVE_THERMAL *c=p;volatile ULONG *r;NTSTATUS status=STATUS_SUCCESS;
 if(!v||!qualified(c)||!c->parent.Begin(c->parent.Header.Context,c->token))return -1;
 if(a==0x10018604u){ULONG analog;status=c->parent.ReadAnalog(c->parent.Header.Context,c->token,&analog);if(NT_SUCCESS(status))*v=(uint32_t)analog;}
 else if((r=address(c,a))!=NULL){*v=READ_REGISTER_ULONG(r);if(*v==MAXULONG)status=STATUS_DEVICE_HARDWARE_ERROR;}
 else status=STATUS_INVALID_PARAMETER;
 c->parent.End(c->parent.Header.Context);return NT_SUCCESS(status)?0:-1;
}
static int write32(void *p,uint32_t a,uint32_t v)
{
 NATIVE_THERMAL *c=p;volatile ULONG *r;NTSTATUS status=STATUS_SUCCESS;
 if(!qualified(c))return -1;
 if(a==0x10001120u&&v==1){c->reset_acked=NT_SUCCESS(c->parent.Reset(c->parent.Header.Context,c->token));return c->reset_acked?0:-1;}
 if(a==0x10001124u&&v==1){if(!c->reset_acked)return -1;c->reset_acked=FALSE;return 0;} /* Parent Reset is an assert/deassert pair. */
 if(!c->parent.Begin(c->parent.Header.Context,c->token))return -1;
 if(a==0x10018604u)status=c->parent.WriteAnalog(c->parent.Header.Context,c->token,v);
 else if((r=address(c,a))!=NULL)WRITE_REGISTER_ULONG(r,v);
 else status=STATUS_INVALID_PARAMETER;
 c->parent.End(c->parent.Header.Context);return NT_SUCCESS(status)?0:-1;
}
static int delay_us(void *p,unsigned us){if(!qualified(p)||!us||us>200)return -1;KeStallExecutionProcessor(us);return 0;}
static uint64_t now_ms(void *p){UNREFERENCED_PARAMETER(p);return KeQueryInterruptTime()/10000u;}
static int wait_ms(void *p,unsigned ms){LARGE_INTEGER d;if(!qualified(p)||!ms||ms>10)return -1;d.QuadPart=-(LONGLONG)ms*10000;return NT_SUCCESS(KeDelayExecutionThread(KernelMode,FALSE,&d))?0:-1;}
static int ready(void *p,int v){NATIVE_THERMAL *c=p;if(!qualified(c))return -1;c->ready=v?TRUE:FALSE;return 0;}
NTSTATUS NativePrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 NATIVE_THERMAL *c=Native(device);struct a04_thermal_resource r[2];ULONG i;PHYSICAL_ADDRESS a;
 if(c->mapped[0]||c->mapped[1]||WdfCmResourceListGetCount(raw)!=2||WdfCmResourceListGetCount(translated)!=2)return STATUS_DEVICE_CONFIGURATION_ERROR;
 RtlZeroMemory(r,sizeof(r));
 for(i=0;i<2;i++){
  PCM_PARTIAL_RESOURCE_DESCRIPTOR x=WdfCmResourceListGetDescriptor(raw,i),y=WdfCmResourceListGetDescriptor(translated,i);
  if(!x||!y||x->Type!=CmResourceTypeMemory||y->Type!=CmResourceTypeMemory)return STATUS_DEVICE_CONFIGURATION_ERROR;
  r[i].raw_memory=r[i].memory=1;r[i].raw_exclusive=x->ShareDisposition==CmResourceShareDeviceExclusive;r[i].exclusive=y->ShareDisposition==CmResourceShareDeviceExclusive;
  r[i].raw=x->u.Memory.Start.QuadPart;r[i].translated=y->u.Memory.Start.QuadPart;r[i].raw_size=x->u.Memory.Length;r[i].size=y->u.Memory.Length;
 }
 if(a04_thermal_resources_validate_shared(r,2))return STATUS_DEVICE_CONFIGURATION_ERROR;
 for(i=0;i<2;i++){a.QuadPart=i?0x11003000:0x1100d000;c->mapped[i]=MmMapIoSpace(a,0x1000,MmNonCached);if(!c->mapped[i]){while(i){--i;MmUnmapIoSpace((PVOID)c->mapped[i],0x1000);c->mapped[i]=NULL;}return STATUS_INSUFFICIENT_RESOURCES;}}
 return STATUS_SUCCESS;
}
NTSTATUS NativeRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
 NATIVE_THERMAL *c=Native(device);unsigned i;UNREFERENCED_PARAMETER(translated);
 if(c->leased||c->uncertain)return STATUS_DEVICE_BUSY;
 drop_interface(c);for(i=0;i<2;i++)if(c->mapped[i]){MmUnmapIoSpace((PVOID)c->mapped[i],0x1000);c->mapped[i]=NULL;}return STATUS_SUCCESS;
}
NTSTATUS NativeD0(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
 NATIVE_THERMAL *c=Native(device);struct a04_thermal_boot_cal v;struct a04_thermal_cal cal;
 struct a04_oneshot_io io;ULONG bytes=sizeof(v),attributes=0;GUID guid=calGuid;
 UNICODE_STRING name=RTL_CONSTANT_STRING(L"A04BR3ThermalCalibration");NTSTATUS status;
 UNREFERENCED_PARAMETER(previous);if(c->uncertain||c->leased||c->bound)return STATUS_DEVICE_NOT_READY;
 InterlockedExchange(&c->stopping,0);RtlZeroMemory(&v,sizeof(v));
 WdfSpinLockAcquire(c->lock);a04_readout_invalidate(&c->reading);WdfSpinLockRelease(c->lock);
 { DECLARE_CONST_UNICODE_STRING(path,A04_UART_SCREEN_DEVICE);WDF_IO_TARGET_OPEN_PARAMS open;
  WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,GENERIC_READ|GENERIC_WRITE);(VOID)WdfIoTargetOpen(c->uart,&open); }
 TRACE(c,"A04THM NATIVE START parent1012\n");
 status=ExGetFirmwareEnvironmentVariable(&name,&guid,&v,&bytes,&attributes);
 if(!NT_SUCCESS(status)||a04_thermal_boot_calibration(&cal,&v,bytes,attributes)){TRACE(c,"A04THM NATIVE CAL REJECT\n");WdfIoTargetClose(c->uart);return STATUS_DEVICE_NOT_READY;}
 status=WdfFdoQueryForInterface(device,&A04SharedThermalNativeGuid,(PINTERFACE)&c->parent,sizeof(c->parent),A04_THERMAL_NATIVE_VERSION,NULL);
 if(!NT_SUCCESS(status)){TRACE(c,"A04THM NATIVE TUPLE REJECT\n");WdfIoTargetClose(c->uart);return status;}
 c->bound=TRUE;
 if(c->parent.Header.Size!=sizeof(c->parent)||c->parent.Header.Version!=A04_THERMAL_NATIVE_VERSION||c->parent.ParentBuild!=1012||
    !c->parent.Header.Context||!c->parent.Header.InterfaceReference||!c->parent.Header.InterfaceDereference||!c->parent.Acquire||!c->parent.Clock||!c->parent.Reset||!c->parent.Begin||!c->parent.End||!c->parent.ReadAnalog||!c->parent.WriteAnalog||!c->parent.Quiescent||!c->parent.Release){
  if(c->parent.Header.InterfaceDereference)drop_interface(c);WdfIoTargetClose(c->uart);return STATUS_REVISION_MISMATCH;
 }

 status=c->parent.Acquire(c->parent.Header.Context,&c->token);
 if(!NT_SUCCESS(status)){TRACE(c,"A04THM NATIVE CLAIM FAILED\n");drop_interface(c);WdfIoTargetClose(c->uart);return status;}
 c->leased=TRUE;TRACE(c,"A04THM NATIVE OS CLAIM parent1012\n");
 c->hw.ctx=c;c->hw.qualified=qualified;c->hw.clocks_on=clocks;c->hw.initialized=initialized;c->hw.read32=read32;c->hw.write32=write32;c->hw.delay_us=delay_us;
 io.hw=&c->hw;io.context=c;io.now_ms=now_ms;io.wait_ms=wait_ms;io.set_ready=ready;io.report=report;
 enum a04_oneshot_result result=a04_thermal_oneshot(&io,&cal);
 if(result==A04_ONESHOT_SHUTDOWN_FAILED){c->uncertain=TRUE;TRACE(c,"A04THM NATIVE UNCERTAIN RETAIN\n");KeBugCheckEx(0xE2,0x41303454,1012,result,0);return STATUS_DEVICE_HARDWARE_ERROR;}
 /* Thermal is stopped. Clear only ADC channel11's trigger and confirm ADC
  * idle before permitting restoration of shared analog/clock differences. */
 { uint32_t busy,trigger;unsigned i;
  if(write32(c,0x1100300cu,0x800u))goto retain;
  for(i=0;i<50;i++){
   if(read32(c,0x11003004u,&trigger)||read32(c,0x11003010u,&busy))goto retain;
   if(!(trigger&0x800u)&&!(busy&1u))break;
   if(delay_us(c,2))goto retain;
  }
  if(i==50)goto retain;
 }
 status=c->parent.Quiescent(c->parent.Header.Context,c->token);
 if(NT_SUCCESS(status))status=c->parent.Release(c->parent.Header.Context,c->token);
 if(!NT_SUCCESS(status)){c->uncertain=TRUE;KeBugCheckEx(0xE2,0x41303454,1012,status,1);return status;}
 c->leased=FALSE;c->ready=FALSE;drop_interface(c);
 if(result==A04_ONESHOT_OK)TRACE(c,"A04THM NATIVE QUIESCENT OK\n");else TRACE(c,"A04THM NATIVE SAMPLE FAILED\n");
 WdfIoTargetClose(c->uart);return result==A04_ONESHOT_OK?STATUS_SUCCESS:STATUS_DEVICE_HARDWARE_ERROR;
retain:
 c->uncertain=TRUE;TRACE(c,"A04THM NATIVE ADC RETAIN\n");KeBugCheckEx(0xE2,0x41303454,1012,3,0);return STATUS_DEVICE_HARDWARE_ERROR;
}
NTSTATUS NativeExit(WDFDEVICE device,WDF_POWER_DEVICE_STATE next)
{ NATIVE_THERMAL *c=Native(device);UNREFERENCED_PARAMETER(next);InterlockedExchange(&c->stopping,1);WdfSpinLockAcquire(c->lock);a04_readout_invalidate(&c->reading);WdfSpinLockRelease(c->lock);if(c->leased||c->uncertain)return STATUS_DEVICE_BUSY;drop_interface(c);WdfIoTargetClose(c->uart);return STATUS_SUCCESS; }
VOID NativeRemoved(WDFDEVICE device){InterlockedExchange(&Native(device)->stopping,1);}
VOID NativeIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outSize,size_t inSize,ULONG code)
{
 NATIVE_THERMAL *c=Native(WdfIoQueueGetDevice(queue));struct a04_thermal_readout_v1 value,*output;NTSTATUS status;UNREFERENCED_PARAMETER(inSize);
 if(code!=A04_THERMAL_GET_READING){WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return;}
 if(outSize<sizeof(value)){WdfRequestComplete(request,STATUS_BUFFER_TOO_SMALL);return;}
 status=WdfRequestRetrieveOutputBuffer(request,sizeof(value),(PVOID *)&output,NULL);if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
 WdfSpinLockAcquire(c->lock);status=a04_readout_get(&c->reading,now_ms(c),1000,&value)?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;if(NT_SUCCESS(status))*output=value;WdfSpinLockRelease(c->lock);
 WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status)?sizeof(value):0);
}
NTSTATUS NativeAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDFDEVICE device;WDF_PNPPOWER_EVENT_CALLBACKS p;WDF_OBJECT_ATTRIBUTES a;WDF_IO_QUEUE_CONFIG q;NTSTATUS status;NATIVE_THERMAL *c;UNREFERENCED_PARAMETER(driver);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);p.EvtDevicePrepareHardware=NativePrepare;p.EvtDeviceReleaseHardware=NativeRelease;p.EvtDeviceD0Entry=NativeD0;p.EvtDeviceD0Exit=NativeExit;p.EvtDeviceSurpriseRemoval=NativeRemoved;WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,NATIVE_THERMAL);a.ExecutionLevel=WdfExecutionLevelPassive;
 status=WdfDeviceCreate(&init,&a,&device);if(!NT_SUCCESS(status))return status;c=Native(device);c->device=device;
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=device;status=WdfSpinLockCreate(&a,&c->lock);if(!NT_SUCCESS(status))return status;
 status=WdfIoTargetCreate(device,&a,&c->uart);if(!NT_SUCCESS(status))return status;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchSequential);q.EvtIoDeviceControl=NativeIoctl;status=WdfIoQueueCreate(device,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(status))return status;
 return WdfDeviceCreateDeviceInterface(device,&A04ThermalReadoutInterfaceGuid,NULL);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{ WDF_DRIVER_CONFIG c;WDF_DRIVER_CONFIG_INIT(&c,NativeAdd);return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE); }
