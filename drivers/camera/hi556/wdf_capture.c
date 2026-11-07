/* SPDX-License-Identifier: GPL-2.0-only
 * Opt-in A04BR3 diagnostic RAW capture device. Not an AVStream/WinRT camera.
 * No receiver/sensor writes until an explicit fixed capture IOCTL. */
#include "windows_capture.h"
#ifndef A04_HI556_CAPTURE_EXPERIMENT
#define A04_HI556_CAPTURE_EXPERIMENT 0
#endif
EVT_WDF_DRIVER_DEVICE_ADD CameraAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE CameraPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE CameraRelease;
EVT_WDF_DEVICE_D0_ENTRY CameraD0;
EVT_WDF_DEVICE_D0_EXIT CameraOff;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL CameraIo;
DRIVER_INITIALIZE DriverEntry;
static void unmap(A04_CAM_CONTEXT *c)
{for(unsigned i=0;i<4;i++)if(c->region[i]){MmUnmapIoSpace(c->region[i],c->region_size[i]);c->region[i]=NULL;}}
NTSTATUS CameraPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 A04_CAM_CONTEXT *c=CameraContext(dev);unsigned memory=0,irq=0;NTSTATUS s=STATUS_DEVICE_CONFIGURATION_ERROR;
 static const ULONG base[]={0x15004000,0x10011000,0x15001000,0};
 static const ULONG size[]={0x9000,0x1000,0x1000,0x1000};
 ULONG count=WdfCmResourceListGetCount(raw);PHYSICAL_ADDRESS start[4];
 if(!A04_HI556_CAPTURE_EXPERIMENT)return STATUS_NOT_SUPPORTED;
 if(count!=WdfCmResourceListGetCount(translated))return s;
 for(ULONG i=0;i<count;i++){
  PCM_PARTIAL_RESOURCE_DESCRIPTOR r=WdfCmResourceListGetDescriptor(raw,i),t=WdfCmResourceListGetDescriptor(translated,i);
  if(!r||!t||r->Type!=t->Type)goto fail;
  if(r->Type==CmResourceTypeMemory){
   if(memory>=4||r->u.Memory.Length!=size[memory]||t->u.Memory.Length!=size[memory]||
      (memory<3&&(r->u.Memory.Start.QuadPart!=base[memory]))||
      (memory==3&&((r->u.Memory.Start.QuadPart&4095)||r->u.Memory.Start.QuadPart<=0)))goto fail;
   if(memory==3&&!(r->Flags&CM_RESOURCE_MEMORY_READ_ONLY))goto fail;
   start[memory]=t->u.Memory.Start;c->region_size[memory]=size[memory];memory++;
  }else if(r->Type==CmResourceTypeInterrupt){
   if(++irq!=1||r->u.Interrupt.Vector!=212||(r->Flags&CM_RESOURCE_INTERRUPT_LATCHED))goto fail;
  }else if(r->Type!=CmResourceTypeDevicePrivate)goto fail;
 }
 if(memory!=4||irq!=1)goto fail;
 for(unsigned i=0;i<4;i++){
  if(start[i].QuadPart<0||start[i].QuadPart>0xffffffffLL-size[i])goto fail;
  for(unsigned j=0;j<i;j++)if(start[i].QuadPart<start[j].QuadPart+size[j]&&start[j].QuadPart<start[i].QuadPart+size[i])goto fail;
  c->region[i]=MmMapIoSpace(start[i],size[i],MmNonCached);if(!c->region[i]){s=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
 }
 s=a04_cam_qualified(c);if(!NT_SUCCESS(s))goto fail;
 return STATUS_SUCCESS;
fail:unmap(c);return s;
}
NTSTATUS CameraD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE previous)
{
 A04_CAM_CONTEXT *c=CameraContext(dev);NTSTATUS s;UNREFERENCED_PARAMETER(previous);
 if(c->quarantined||c->leases||c->generation==MAXULONG)return STATUS_DEVICE_NOT_READY;
 s=a04_cam_qualified(c);if(!NT_SUCCESS(s))return s;
 c->generation++;a04_cam_power_init(c);InterlockedExchange(&c->online,1);
 s=a04_hi556_owner_open(&c->identity);if(!NT_SUCCESS(s))goto fail;
 s=a04_cam_identify(c);if(!NT_SUCCESS(s))goto fail;
 s=a04_hi556_camera_open(&c->sensor);if(!NT_SUCCESS(s))goto fail;
 return STATUS_SUCCESS;
fail:
 if(c->leases){c->quarantined=TRUE;WdfObjectReference(dev);return s;}
 InterlockedExchange(&c->online,0);a04_hi556_owner_close(&c->identity);return s;
}
NTSTATUS CameraOff(WDFDEVICE dev,WDF_POWER_DEVICE_STATE target)
{
 A04_CAM_CONTEXT *c=CameraContext(dev);UNREFERENCED_PARAMETER(target);
 /* The power-managed sequential queue has completed its bounded request.
  * Capture always stops/ACKs DMA before completion, including cancellation. */
 if(c->quarantined||c->leases||c->status.receiver.armed){
  if(!c->quarantined){c->quarantined=TRUE;WdfObjectReference(dev);}return STATUS_DEVICE_BUSY;
 }
 InterlockedExchange(&c->online,0);a04_hi556_camera_close(&c->sensor);a04_hi556_owner_close(&c->identity);
 a04_cam_free_dma(c);return STATUS_SUCCESS;
}
NTSTATUS CameraRelease(WDFDEVICE dev,WDFCMRESLIST translated)
{
 A04_CAM_CONTEXT *c=CameraContext(dev);UNREFERENCED_PARAMETER(translated);
 if(c->quarantined||c->leases||c->status.receiver.armed)return STATUS_DEVICE_BUSY;
 a04_cam_free_dma(c);unmap(c);return STATUS_SUCCESS;
}
VOID CameraIo(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG code)
{
 A04_CAM_CONTEXT *c=CameraContext(WdfIoQueueGetDevice(queue));PVOID buffer=NULL;NTSTATUS s;ULONG_PTR used=0;
 if(input||!A04_HI556_CAPTURE_EXPERIMENT){WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;}
 if(code==A04_CAM_IOCTL_CAPTURE){
  s=WdfRequestRetrieveOutputBuffer(request,sizeof(A04_CAM_STATUS)+A04_HI556_FRAME_BYTES,&buffer,NULL);
  if(NT_SUCCESS(s))s=a04_cam_frame(c,request,buffer,output,&used);
 }else if(code==A04_CAM_IOCTL_STATUS){
  s=WdfRequestRetrieveOutputBuffer(request,sizeof(A04_CAM_STATUS),&buffer,NULL);
  if(NT_SUCCESS(s)){RtlCopyMemory(buffer,&c->status,sizeof(c->status));used=sizeof(c->status);}
 }else s=STATUS_INVALID_DEVICE_REQUEST;
 WdfRequestCompleteWithInformation(request,s,NT_SUCCESS(s)?used:0);
}
NTSTATUS CameraAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS callbacks;WDF_OBJECT_ATTRIBUTES attr;WDFDEVICE dev;WDF_IO_QUEUE_CONFIG queue;NTSTATUS s;
 UNREFERENCED_PARAMETER(driver);if(!A04_HI556_CAPTURE_EXPERIMENT)return STATUS_NOT_SUPPORTED;
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_UNKNOWN);WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN,TRUE);
 WdfDeviceInitSetExclusive(init,TRUE);WdfDeviceInitSetIoType(init,WdfDeviceIoDirect);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);callbacks.EvtDevicePrepareHardware=CameraPrepare;callbacks.EvtDeviceReleaseHardware=CameraRelease;
 callbacks.EvtDeviceD0Entry=CameraD0;callbacks.EvtDeviceD0Exit=CameraOff;WdfDeviceInitSetPnpPowerEventCallbacks(init,&callbacks);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,A04_CAM_CONTEXT);attr.ExecutionLevel=WdfExecutionLevelPassive;
 s=WdfDeviceCreate(&init,&attr,&dev);if(!NT_SUCCESS(s))return s;CameraContext(dev)->device=dev;
 s=WdfDeviceCreateDeviceInterface(dev,&A04Hi556CaptureGuid,NULL);if(!NT_SUCCESS(s))return s;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchSequential);queue.EvtIoDeviceControl=CameraIo;
 return WdfIoQueueCreate(dev,&queue,WDF_NO_OBJECT_ATTRIBUTES,NULL);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{
 WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,CameraAdd);
 return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
