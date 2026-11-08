/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT6392 power/volume-down keys, using the existing single PWRAP supplier.
 * Standard mshidkmdf lower filter; no MMIO, register writes, IRQ or wake.
 * The legacy MT8167 keypad range on this PDO is checked but never mapped. */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include "../a04br3-buttons/shared_parent_contract.h"
#include "state.h"
typedef struct {A04_SHARED_INTERFACE parent;WDFWAITLOCK lock;WDFQUEUE reads;WDFTIMER timer;
 struct mt6392_keys_state state;UCHAR pending;BOOLEAN online;} KEYS_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(KEYS_CONTEXT,Keys)
EVT_WDF_DRIVER_DEVICE_ADD KeysAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE KeysPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE KeysRelease;
EVT_WDF_DEVICE_D0_ENTRY KeysD0;
EVT_WDF_DEVICE_D0_EXIT KeysExit;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL KeysIoctl;
EVT_WDF_TIMER KeysTimer;
static NTSTATUS Copy(WDFREQUEST request,const void *data,size_t bytes) {
 PVOID out;NTSTATUS s=WdfRequestRetrieveOutputBuffer(request,bytes,&out,NULL);
 if(NT_SUCCESS(s)){RtlCopyMemory(out,data,bytes);WdfRequestSetInformation(request,bytes);}return s;
}
static VOID Drain(KEYS_CONTEXT *c) {
 unsigned budget;
 for(budget=0;budget<4;++budget){WDFREQUEST request;NTSTATUS s;UCHAR data[2];unsigned id;
  WdfWaitLockAcquire(c->lock,NULL);
  if(!c->pending||!NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->reads,&request))){WdfWaitLockRelease(c->lock);return;}
  id=(c->pending&1)?1:2;mt6392_keys_report(&c->state,id,data);s=Copy(request,data,sizeof(data));
  if(NT_SUCCESS(s))c->pending&=(UCHAR)~(1u<<(id-1));
  WdfWaitLockRelease(c->lock);WdfRequestComplete(request,s);
 }
}
NTSTATUS KeysPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated) {
 KEYS_CONTEXT *c=Keys(device);PCM_PARTIAL_RESOURCE_DESCRIPTOR a,b;NTSTATUS s;
 if(c->parent.Context||WdfCmResourceListGetCount(raw)!=1||WdfCmResourceListGetCount(translated)!=1)return STATUS_DEVICE_CONFIGURATION_ERROR;
 a=WdfCmResourceListGetDescriptor(raw,0);b=WdfCmResourceListGetDescriptor(translated,0);
 if(!a||!b||a->Type!=CmResourceTypeMemory||b->Type!=CmResourceTypeMemory||b->u.Memory.Start.QuadPart<=0||
 !a04_button_shared_keypad_resource((uint64_t)a->u.Memory.Start.QuadPart,a->u.Memory.Length,a->ShareDisposition==CmResourceShareDeviceExclusive,
 (uint64_t)b->u.Memory.Start.QuadPart,b->u.Memory.Length,b->ShareDisposition==CmResourceShareDeviceExclusive))return STATUS_DEVICE_CONFIGURATION_ERROR;
 RtlZeroMemory(&c->parent,sizeof(c->parent));s=WdfFdoQueryForInterface(device,&A04SharedInterfaceGuid,(PINTERFACE)&c->parent,sizeof(c->parent),A04_SHARED_INTERFACE_VERSION,NULL);
 if(!NT_SUCCESS(s)){RtlZeroMemory(&c->parent,sizeof(c->parent));return s;}
 if(!a04_button_shared_interface_valid(&c->parent)){
  if(c->parent.Context&&c->parent.InterfaceDereference)c->parent.InterfaceDereference(c->parent.Context);
  RtlZeroMemory(&c->parent,sizeof(c->parent));return STATUS_REVISION_MISMATCH;}
 return STATUS_SUCCESS;
}
NTSTATUS KeysRelease(WDFDEVICE device,WDFCMRESLIST translated) {
 KEYS_CONTEXT *c=Keys(device);UNREFERENCED_PARAMETER(translated);
 if(c->parent.Context){c->parent.InterfaceDereference(c->parent.Context);RtlZeroMemory(&c->parent,sizeof(c->parent));}return STATUS_SUCCESS;
}
NTSTATUS KeysD0(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous) {
 KEYS_CONTEXT *c=Keys(device);UNREFERENCED_PARAMETER(previous);
 if(!c->parent.Context)return STATUS_DEVICE_NOT_READY;
 WdfIoQueueStart(c->reads);WdfWaitLockAcquire(c->lock,NULL);mt6392_keys_init(&c->state);c->pending=3;c->online=TRUE;WdfWaitLockRelease(c->lock);
 WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(20));return STATUS_SUCCESS;
}
NTSTATUS KeysExit(WDFDEVICE device,WDF_POWER_DEVICE_STATE next) {
 KEYS_CONTEXT *c=Keys(device);UNREFERENCED_PARAMETER(next);
 WdfWaitLockAcquire(c->lock,NULL);c->online=FALSE;WdfWaitLockRelease(c->lock);
 WdfTimerStop(c->timer,TRUE);WdfTimerStop(c->timer,FALSE);
 WdfWaitLockAcquire(c->lock,NULL);c->state.value=0;c->pending=3;WdfWaitLockRelease(c->lock);
 Drain(c);WdfIoQueuePurgeSynchronously(c->reads);return STATUS_SUCCESS;
}
VOID KeysTimer(WDFTIMER timer) {
 KEYS_CONTEXT *c=Keys((WDFDEVICE)WdfTimerGetParentObject(timer));USHORT raw=0;int rearm=0,valid;
 WdfWaitLockAcquire(c->lock,NULL);
 if(c->online&&!c->state.failed){
  valid=c->parent.ButtonPmicRead(c->parent.Context,&raw)==0;
  if(mt6392_keys_sample(&c->state,valid,raw))c->pending=3;
  rearm=!c->state.failed;
  if(!valid)DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT6392KEYS supplier read failed; input released and stopped\n");
 }
 WdfWaitLockRelease(c->lock);Drain(c);
 /* D0Exit closes online under this lock, then synchronously cancels the timer. */
 WdfWaitLockAcquire(c->lock,NULL);if(c->online&&rearm)WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(20));WdfWaitLockRelease(c->lock);
}
VOID KeysIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code) {
 KEYS_CONTEXT *c=Keys(WdfIoQueueGetDevice(queue));HID_DESCRIPTOR hid={0};HID_DEVICE_ATTRIBUTES attr={0};NTSTATUS s=STATUS_NOT_SUPPORTED;
 UNREFERENCED_PARAMETER(inlen);UNREFERENCED_PARAMETER(outlen);
 switch(code){
 case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
  hid.bLength=sizeof(hid);hid.bDescriptorType=HID_HID_DESCRIPTOR_TYPE;hid.bcdHID=0x0111;hid.bNumDescriptors=1;
  hid.DescriptorList[0].bReportType=HID_REPORT_DESCRIPTOR_TYPE;hid.DescriptorList[0].wReportLength=sizeof(mt6392_keys_descriptor);s=Copy(request,&hid,sizeof(hid));break;
 case IOCTL_HID_GET_REPORT_DESCRIPTOR:s=Copy(request,mt6392_keys_descriptor,sizeof(mt6392_keys_descriptor));break;
 case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
  /* Software-enumerated parent child: no invented USB VID/PID identity. */
  attr.Size=sizeof(attr);attr.VersionNumber=1;s=Copy(request,&attr,sizeof(attr));break;
 case IOCTL_HID_READ_REPORT:
  WdfWaitLockAcquire(c->lock,NULL);s=c->online?WdfRequestForwardToIoQueue(request,c->reads):STATUS_DEVICE_NOT_READY;WdfWaitLockRelease(c->lock);
  if(NT_SUCCESS(s)){Drain(c);return;}break;
 case IOCTL_HID_GET_INPUT_REPORT:{PHID_XFER_PACKET p=(PHID_XFER_PACKET)WdfRequestWdmGetIrp(request)->UserBuffer;UCHAR report[2];
  if(outlen<sizeof(*p)||!p||!p->reportBuffer||p->reportBufferLen<2||(p->reportId!=1&&p->reportId!=2)){s=STATUS_INVALID_PARAMETER;break;}
  WdfWaitLockAcquire(c->lock,NULL);mt6392_keys_report(&c->state,p->reportId,report);WdfWaitLockRelease(c->lock);
  RtlCopyMemory(p->reportBuffer,report,2);WdfRequestSetInformation(request,2);s=STATUS_SUCCESS;break;}
 }
 WdfRequestComplete(request,s);
}
NTSTATUS KeysAdd(WDFDRIVER driver,PWDFDEVICE_INIT init) {
 WDFDEVICE dev;KEYS_CONTEXT *c;WDF_OBJECT_ATTRIBUTES a;WDF_PNPPOWER_EVENT_CALLBACKS p;WDF_IO_QUEUE_CONFIG q;WDF_TIMER_CONFIG t;NTSTATUS s;UNREFERENCED_PARAMETER(driver);
 WdfFdoInitSetFilter(init);WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);p.EvtDevicePrepareHardware=KeysPrepare;p.EvtDeviceReleaseHardware=KeysRelease;p.EvtDeviceD0Entry=KeysD0;p.EvtDeviceD0Exit=KeysExit;WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,KEYS_CONTEXT);a.ExecutionLevel=WdfExecutionLevelPassive;a.SynchronizationScope=WdfSynchronizationScopeNone;
 s=WdfDeviceCreate(&init,&a,&dev);if(!NT_SUCCESS(s))return s;c=Keys(dev);
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=dev;s=WdfWaitLockCreate(&a,&c->lock);if(!NT_SUCCESS(s))return s;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);q.PowerManaged=WdfFalse;q.EvtIoInternalDeviceControl=KeysIoctl;s=WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(s))return s;
 WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual);q.PowerManaged=WdfFalse;s=WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->reads);if(!NT_SUCCESS(s))return s;
 WDF_TIMER_CONFIG_INIT(&t,KeysTimer);t.AutomaticSerialization=FALSE;
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=dev;a.ExecutionLevel=WdfExecutionLevelPassive;return WdfTimerCreate(&t,&a,&c->timer);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path){WDF_DRIVER_CONFIG c;WDF_DRIVER_CONFIG_INIT(&c,KeysAdd);return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE);}
