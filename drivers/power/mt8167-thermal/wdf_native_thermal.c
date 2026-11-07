/* SPDX-License-Identifier: GPL-2.0-only
 * Parent1018 owns the complete synchronous PIO transaction and quarantine.
 * The child maps its exclusive resources and explicitly transfers ownership;
 * it never accesses a thermal/ADC register or frees adopted mappings. */
#include <ntddk.h>
#include <wdf.h>
#include "resources.h"
#include "boot_record.h"
#include "readout.h"
#include "readout_ioctl.h"
#include "../../bus/mt8167-shared/thermal_native_interface.h"
#include "../../system/mt8167-toprgu/uart_screen.h"
typedef struct {PVOID mapped[2];WDFSPINLOCK lock;WDFIOTARGET uart;struct a04_thermal_readout_state reading;} PIO_THERMAL;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PIO_THERMAL,Pio)
EVT_WDF_DRIVER_DEVICE_ADD PioAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE PioPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE PioRelease;
EVT_WDF_DEVICE_D0_ENTRY PioD0;
EVT_WDF_DEVICE_D0_EXIT PioExit;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL PioIoctl;
static const GUID calGuid={0x92d56a32,0xc847,0x4fdc,{0x83,0x62,0xf2,0x77,0xc7,0x10,0xa0,0x43}};
static void mark(PIO_THERMAL *c,const char *s,ULONG n) {
 WDF_MEMORY_DESCRIPTOR input;WDF_REQUEST_SEND_OPTIONS options;
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"%.*s",(int)n,s);
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&input,(PVOID)s,n);WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
 WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(1));
 (VOID)WdfIoTargetSendIoctlSynchronously(c->uart,NULL,A04_UART_SCREEN_SEND,&input,NULL,&options,NULL);
}
#define MARK(c,s) mark(c,s,sizeof(s)-1)
static void number(char *s,ULONG *n,LONG value) {
 char digits[12];unsigned count=0;ULONG u;if(value<0){s[(*n)++]='-';u=0u-(ULONG)value;}else u=(ULONG)value;
 do{digits[count++]=(char)('0'+u%10);u/=10;}while(u);while(count)s[(*n)++]=digits[--count];
}
NTSTATUS PioPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated) {
 PIO_THERMAL *c=Pio(device);struct a04_thermal_resource r[2];unsigned i;PHYSICAL_ADDRESS a;
 if(c->mapped[0]||c->mapped[1]||WdfCmResourceListGetCount(raw)!=2||WdfCmResourceListGetCount(translated)!=2)return STATUS_DEVICE_CONFIGURATION_ERROR;
 RtlZeroMemory(r,sizeof(r));
 for(i=0;i<2;i++) {PCM_PARTIAL_RESOURCE_DESCRIPTOR x=WdfCmResourceListGetDescriptor(raw,i),y=WdfCmResourceListGetDescriptor(translated,i);
  if(!x||!y||x->Type!=CmResourceTypeMemory||y->Type!=CmResourceTypeMemory)return STATUS_DEVICE_CONFIGURATION_ERROR;
  r[i].raw_memory=r[i].memory=1;r[i].raw_exclusive=x->ShareDisposition==CmResourceShareDeviceExclusive;r[i].exclusive=y->ShareDisposition==CmResourceShareDeviceExclusive;
  r[i].raw=x->u.Memory.Start.QuadPart;r[i].translated=y->u.Memory.Start.QuadPart;r[i].raw_size=x->u.Memory.Length;r[i].size=y->u.Memory.Length;}
 if(a04_thermal_resources_validate_shared(r,2))return STATUS_DEVICE_CONFIGURATION_ERROR;
 for(i=0;i<2;i++) {a.QuadPart=i?0x11003000:0x1100d000;c->mapped[i]=MmMapIoSpace(a,0x1000,MmNonCached);
  if(!c->mapped[i]){while(i){--i;MmUnmapIoSpace(c->mapped[i],0x1000);c->mapped[i]=NULL;}return STATUS_INSUFFICIENT_RESOURCES;}}
 return STATUS_SUCCESS;
}
NTSTATUS PioRelease(WDFDEVICE device,WDFCMRESLIST translated) {
 PIO_THERMAL *c=Pio(device);unsigned i;UNREFERENCED_PARAMETER(translated);
 for(i=0;i<2;i++)if(c->mapped[i]){MmUnmapIoSpace(c->mapped[i],0x1000);c->mapped[i]=NULL;}
 return STATUS_SUCCESS; /* Only mappings never adopted by hardware owner. */
}
NTSTATUS PioD0(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous) {
 PIO_THERMAL *c=Pio(device);struct a04_thermal_boot_cal v;struct a04_thermal_cal cal;struct a04_thermal_sample sample;
 A04_THERMAL_NATIVE_INTERFACE iface;ULONG raw[3],adopted=0,stage=0,bytes=sizeof(v),attrs=0;GUID guid=calGuid;NTSTATUS status;
 UNICODE_STRING name=RTL_CONSTANT_STRING(L"A04BR3ThermalCalibration");UNREFERENCED_PARAMETER(previous);
 WdfSpinLockAcquire(c->lock);a04_readout_invalidate(&c->reading);WdfSpinLockRelease(c->lock);
 if(!c->mapped[0]||!c->mapped[1])return STATUS_DEVICE_NOT_READY;
 {DECLARE_CONST_UNICODE_STRING(path,A04_UART_SCREEN_DEVICE);WDF_IO_TARGET_OPEN_PARAMS open;
  WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,GENERIC_READ|GENERIC_WRITE);(VOID)WdfIoTargetOpen(c->uart,&open);}
 MARK(c,"A04THM PIO START parent1018\n");
 RtlZeroMemory(&v,sizeof(v));status=ExGetFirmwareEnvironmentVariable(&name,&guid,&v,&bytes,&attrs);
 if(!NT_SUCCESS(status)||a04_thermal_boot_calibration(&cal,&v,bytes,attrs)){MARK(c,"A04THM PIO CAL REJECT\n");status=STATUS_DEVICE_NOT_READY;goto done;}
 RtlZeroMemory(&iface,sizeof(iface));status=WdfFdoQueryForInterface(device,&A04SharedThermalNativeGuid,(PINTERFACE)&iface,sizeof(iface),A04_THERMAL_NATIVE_VERSION,NULL);
 if(!NT_SUCCESS(status)){MARK(c,"A04THM PIO TUPLE REJECT\n");goto done;}
 if(iface.Header.Size!=sizeof(iface)||iface.Header.Version!=2||iface.ParentBuild!=1018||!iface.Header.Context||!iface.Measure||!iface.Header.InterfaceDereference) {
  if(iface.Header.InterfaceDereference)iface.Header.InterfaceDereference(iface.Header.Context);status=STATUS_REVISION_MISMATCH;goto done;}
 status=iface.Measure(iface.Header.Context,c->mapped[0],c->mapped[1],&adopted,raw,&stage);
 if(adopted==1)c->mapped[0]=c->mapped[1]=NULL; /* supplier has the sole lifetime */
 iface.Header.InterfaceDereference(iface.Header.Context);
 if(!NT_SUCCESS(status)){
  static const char hex[]="0123456789ABCDEF";char line[80]="A04THM PIO FAILED status=";ULONG n=sizeof("A04THM PIO FAILED status=")-1;unsigned i;
  for(i=0;i<8;i++)line[n++]=hex[((ULONG)status>>(28-4*i))&15];
  line[n++]=' ';number(line,&n,(LONG)stage);line[n++]='/';number(line,&n,(LONG)adopted);line[n++]='\n';mark(c,line,n);goto done;}
 if(adopted!=1||a04_thermal_sample(&cal,raw,KeQueryInterruptTime()/10000u,1,&sample)){status=STATUS_DEVICE_HARDWARE_ERROR;goto done;}
 WdfSpinLockAcquire(c->lock);int publish=a04_readout_publish(&c->reading,&sample);WdfSpinLockRelease(c->lock);
 if(publish){status=STATUS_DEVICE_HARDWARE_ERROR;goto done;}
 {char line[96]="A04THM PIO mc=";ULONG n=sizeof("A04THM PIO mc=")-1;unsigned i;
  for(i=0;i<3;i++){number(line,&n,sample.mc[i]);line[n++]=i==2?'\n':'/';}mark(c,line,n);}
 MARK(c,"A04THM PIO IDLE restored parent1018\n");
done:
 WdfIoTargetClose(c->uart);return status;
}
NTSTATUS PioExit(WDFDEVICE device,WDF_POWER_DEVICE_STATE next) {
 PIO_THERMAL *c=Pio(device);UNREFERENCED_PARAMETER(next);WdfSpinLockAcquire(c->lock);a04_readout_invalidate(&c->reading);WdfSpinLockRelease(c->lock);return STATUS_SUCCESS;
}
VOID PioIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outputSize,size_t inputSize,ULONG code) {
 PIO_THERMAL *c=Pio(WdfIoQueueGetDevice(queue));struct a04_thermal_readout_v1 v,*out;NTSTATUS status;UNREFERENCED_PARAMETER(inputSize);
 if(code!=A04_THERMAL_GET_READING){WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return;}
 if(outputSize<sizeof(v)){WdfRequestComplete(request,STATUS_BUFFER_TOO_SMALL);return;}
 status=WdfRequestRetrieveOutputBuffer(request,sizeof(v),(PVOID *)&out,NULL);if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
 WdfSpinLockAcquire(c->lock);status=a04_readout_get(&c->reading,KeQueryInterruptTime()/10000u,1000,&v)?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;
 if(NT_SUCCESS(status))*out=v;WdfSpinLockRelease(c->lock);WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status)?sizeof(v):0);
}
NTSTATUS PioAdd(WDFDRIVER driver,PWDFDEVICE_INIT init) {
 WDFDEVICE dev;WDF_OBJECT_ATTRIBUTES a;WDF_PNPPOWER_EVENT_CALLBACKS p;WDF_IO_QUEUE_CONFIG q;NTSTATUS status;PIO_THERMAL *c;UNREFERENCED_PARAMETER(driver);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);p.EvtDevicePrepareHardware=PioPrepare;p.EvtDeviceReleaseHardware=PioRelease;p.EvtDeviceD0Entry=PioD0;p.EvtDeviceD0Exit=PioExit;WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,PIO_THERMAL);a.ExecutionLevel=WdfExecutionLevelPassive;status=WdfDeviceCreate(&init,&a,&dev);if(!NT_SUCCESS(status))return status;c=Pio(dev);
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=dev;status=WdfSpinLockCreate(&a,&c->lock);if(!NT_SUCCESS(status))return status;
 status=WdfIoTargetCreate(dev,&a,&c->uart);if(!NT_SUCCESS(status))return status;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchSequential);q.EvtIoDeviceControl=PioIoctl;status=WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(status))return status;
 return WdfDeviceCreateDeviceInterface(dev,&A04ThermalReadoutInterfaceGuid,NULL);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path) {WDF_DRIVER_CONFIG c;WDF_DRIVER_CONFIG_INIT(&c,PioAdd);return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE);}
