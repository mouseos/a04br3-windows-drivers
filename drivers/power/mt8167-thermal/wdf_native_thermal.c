/* SPDX-License-Identifier: GPL-2.0-only
 * Parent1104 owns the complete synchronous PIO transaction and quarantine.
 * Resource-less child never maps, owns or frees thermal/ADC register ranges. */
#include <ntddk.h>
#include <wdf.h>
#include "boot_record.h"
#include "readout.h"
#include "readout_ioctl.h"
#include "first_read.h"
#include "../../bus/mt8167-shared/thermal_parent_pio_interface.h"
#include "../../system/mt8167-toprgu/uart_screen.h"
typedef struct {WDFSPINLOCK lock;WDFIOTARGET uart;struct a04_thermal_readout_state reading;struct a04_thermal_first_read first;} PIO_THERMAL;
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
 UNREFERENCED_PARAMETER(device);
 return WdfCmResourceListGetCount(raw)==0&&WdfCmResourceListGetCount(translated)==0?STATUS_SUCCESS:STATUS_DEVICE_CONFIGURATION_ERROR;
}
NTSTATUS PioRelease(WDFDEVICE device,WDFCMRESLIST translated) {
 UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(translated);return STATUS_SUCCESS;
}

static int32_t PioCollect(void *opaque) {
 WDFDEVICE device=(WDFDEVICE)opaque;
 PIO_THERMAL *c=Pio(device);struct a04_thermal_boot_cal v;struct a04_thermal_cal cal;struct a04_thermal_sample sample;
 MT8167_THERMAL_PARENT_PIO_INTERFACE iface;ULONG raw[3],adopted=0,stage=0,bytes=sizeof(v),attrs=0;GUID guid=calGuid;NTSTATUS status;
 UNICODE_STRING name=RTL_CONSTANT_STRING(L"A04BR3ThermalCalibration");
 WdfSpinLockAcquire(c->lock);a04_readout_invalidate(&c->reading);WdfSpinLockRelease(c->lock);
 {DECLARE_CONST_UNICODE_STRING(path,A04_UART_SCREEN_DEVICE);WDF_IO_TARGET_OPEN_PARAMS open;
  WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,GENERIC_READ|GENERIC_WRITE);(VOID)WdfIoTargetOpen(c->uart,&open);}
 MARK(c,"MT8167 THERMAL READ BEGIN\n");
 MARK(c,"MT8167 THERMAL CAL READ BEGIN\n");
 RtlZeroMemory(&v,sizeof(v));status=ExGetFirmwareEnvironmentVariable(&name,&guid,&v,&bytes,&attrs);
 if(!NT_SUCCESS(status)||a04_thermal_boot_calibration(&cal,&v,bytes,attrs)){MARK(c,"MT8167 THERMAL CAL REJECT\n");status=STATUS_DEVICE_NOT_READY;goto done;}
 MARK(c,"MT8167 THERMAL CAL READY; PARENT QUERY BEGIN\n");
 RtlZeroMemory(&iface,sizeof(iface));status=WdfFdoQueryForInterface(device,&Mt8167ThermalParentPioGuid,(PINTERFACE)&iface,sizeof(iface),MT8167_THERMAL_PARENT_PIO_VERSION,NULL);
 if(!NT_SUCCESS(status)){MARK(c,"MT8167 THERMAL TUPLE REJECT\n");goto done;}
 if(iface.Header.Size!=sizeof(iface)||iface.Header.Version!=MT8167_THERMAL_PARENT_PIO_VERSION||iface.ParentBuild!=MT8167_THERMAL_PARENT_PIO_BUILD||!iface.Header.Context||!iface.Measure||!iface.Header.InterfaceReference||!iface.Header.InterfaceDereference) {
  if(iface.Header.InterfaceDereference)iface.Header.InterfaceDereference(iface.Header.Context);status=STATUS_REVISION_MISMATCH;goto done;}
 MARK(c,"MT8167 THERMAL PARENT QUERY READY; ADC MEASURE BEGIN\n");
 status=iface.Measure(iface.Header.Context,&adopted,raw,&stage);
 iface.Header.InterfaceDereference(iface.Header.Context);
 if(!NT_SUCCESS(status)){
  static const char hex[]="0123456789ABCDEF";char line[80]="MT8167 THERMAL FAILED status=";ULONG n=sizeof("MT8167 THERMAL FAILED status=")-1;unsigned i;
  for(i=0;i<8;i++)line[n++]=hex[((ULONG)status>>(28-4*i))&15];
  line[n++]=' ';number(line,&n,(LONG)stage);line[n++]='/';number(line,&n,(LONG)adopted);line[n++]='\n';mark(c,line,n);goto done;}
 if(adopted!=1||a04_thermal_sample(&cal,raw,KeQueryInterruptTime()/10000u,1,&sample)){status=STATUS_DEVICE_HARDWARE_ERROR;goto done;}
 WdfSpinLockAcquire(c->lock);int publish=a04_readout_publish(&c->reading,&sample);WdfSpinLockRelease(c->lock);
 if(publish){status=STATUS_DEVICE_HARDWARE_ERROR;goto done;}
 {char line[96]="MT8167 THERMAL mc=";ULONG n=sizeof("MT8167 THERMAL mc=")-1;unsigned i;
  for(i=0;i<3;i++){number(line,&n,sample.mc[i]);line[n++]=i==2?'\n':'/';}mark(c,line,n);}
 MARK(c,"MT8167 THERMAL PARENT ADC RELEASED\n");
done:
 {static const char hex[]="0123456789ABCDEF";char line[64]="MT8167 THERMAL COLLECT END status=";ULONG n=sizeof("MT8167 THERMAL COLLECT END status=")-1;
  for(unsigned i=0;i<8;i++)line[n++]=hex[((ULONG)status>>(28-4*i))&15];line[n++]='\n';mark(c,line,n);}
 WdfIoTargetClose(c->uart);return status;
}
NTSTATUS PioD0(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous) {
 PIO_THERMAL *c=Pio(device);UNREFERENCED_PARAMETER(previous);
 WdfSpinLockAcquire(c->lock);a04_readout_invalidate(&c->reading);WdfSpinLockRelease(c->lock);
 /* A resource-less endpoint is ready to accept READ, not yet a temperature.
  * The power-managed sequential queue runs collection in D0/PASSIVE_LEVEL.
  * Never reset first.attempted on D0/restart: parent remains boot one-shot. */
 {DECLARE_CONST_UNICODE_STRING(path,A04_UART_SCREEN_DEVICE);WDF_IO_TARGET_OPEN_PARAMS open;
  WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,GENERIC_READ|GENERIC_WRITE);(VOID)WdfIoTargetOpen(c->uart,&open);}
 MARK(c,"MT8167 THERMAL INTERFACE D0 READY; FIRST READ PENDING\n");WdfIoTargetClose(c->uart);
 return STATUS_SUCCESS;
}
NTSTATUS PioExit(WDFDEVICE device,WDF_POWER_DEVICE_STATE next) {
 PIO_THERMAL *c=Pio(device);UNREFERENCED_PARAMETER(next);WdfSpinLockAcquire(c->lock);a04_readout_invalidate(&c->reading);WdfSpinLockRelease(c->lock);return STATUS_SUCCESS;
}
VOID PioIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outputSize,size_t inputSize,ULONG code) {
 PIO_THERMAL *c=Pio(WdfIoQueueGetDevice(queue));struct a04_thermal_readout_v1 v,*out;NTSTATUS status;
 if(code!=A04_THERMAL_GET_READING){WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return;}
 if(inputSize){WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;}
 if(outputSize<sizeof(v)){WdfRequestComplete(request,STATUS_BUFFER_TOO_SMALL);return;}
 status=WdfRequestRetrieveOutputBuffer(request,sizeof(v),(PVOID *)&out,NULL);if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
 status=(NTSTATUS)a04_thermal_collect_once(&c->first,WdfIoQueueGetDevice(queue),PioCollect);
 if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
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
