/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Replaces Mt6392Keys on its single existing child. MT8167 exclusive keypad
 * MMIO only; PMIC access remains through the existing shared supplier.
 * Polling, no IRQ/wake, no TOP/INFRA/GPIO/PWRAP or I2C ownership. */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include "../a04br3-buttons/shared_parent_contract.h"
#include "state.h"
#include "checkpoint.h"
typedef struct {A04_SHARED_INTERFACE parent;WDFWAITLOCK lock;WDFQUEUE reads;WDFTIMER timer;
 struct mt8167_keypad_state state;PUCHAR keypad;USHORT saved_enable,saved_debounce;
 UCHAR pending;BOOLEAN online,scan_owned;ULONG scan_reason;} KEYS_CONTEXT;
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
  for(id=1;id<=4;++id)if(c->pending&(1u<<(id-1)))break;mt8167_keypad_report(&c->state,id,data);s=Copy(request,data,sizeof(data));
  if(NT_SUCCESS(s))c->pending&=(UCHAR)~(1u<<(id-1));
  WdfWaitLockRelease(c->lock);
  KeysCheckpoint(KEYS_CP_READ_DELIVERY,KEYS_CP_NTSTATUS,(ULONG)s,id,data[1],NT_SUCCESS(s)?sizeof(data):0,0,0,0);
  if(!NT_SUCCESS(s))KeysCheckpoint(KEYS_CP_READ_FAILURE,KEYS_CP_NTSTATUS,(ULONG)s,id,0,0,0,0,0);
  WdfRequestComplete(request,s);
 }
}
static NTSTATUS KeysPrepareCore(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated) {
 KEYS_CONTEXT *c=Keys(device);PCM_PARTIAL_RESOURCE_DESCRIPTOR a,b;NTSTATUS s;
 if(c->parent.Context||WdfCmResourceListGetCount(raw)!=1||WdfCmResourceListGetCount(translated)!=1)return STATUS_DEVICE_CONFIGURATION_ERROR;
 a=WdfCmResourceListGetDescriptor(raw,0);b=WdfCmResourceListGetDescriptor(translated,0);
 KeysCheckpoint(KEYS_CP_RAW,KEYS_CP_UNOBSERVED,0,a?a->Type:0,a?a->ShareDisposition:0,
  a&&a->Type==CmResourceTypeMemory?a->u.Memory.Start.LowPart:0,a&&a->Type==CmResourceTypeMemory?(ULONG)a->u.Memory.Start.HighPart:0,a&&a->Type==CmResourceTypeMemory?a->u.Memory.Length:0,a!=NULL);
 KeysCheckpoint(KEYS_CP_TRANSLATED,KEYS_CP_UNOBSERVED,0,b?b->Type:0,b?b->ShareDisposition:0,
  b&&b->Type==CmResourceTypeMemory?b->u.Memory.Start.LowPart:0,b&&b->Type==CmResourceTypeMemory?(ULONG)b->u.Memory.Start.HighPart:0,b&&b->Type==CmResourceTypeMemory?b->u.Memory.Length:0,b!=NULL);
 if(!a||!b||a->Type!=CmResourceTypeMemory||b->Type!=CmResourceTypeMemory||b->u.Memory.Start.QuadPart<=0||
 !a04_button_shared_keypad_resource((uint64_t)a->u.Memory.Start.QuadPart,a->u.Memory.Length,a->ShareDisposition==CmResourceShareDeviceExclusive,
 (uint64_t)b->u.Memory.Start.QuadPart,b->u.Memory.Length,b->ShareDisposition==CmResourceShareDeviceExclusive))return STATUS_DEVICE_CONFIGURATION_ERROR;
 RtlZeroMemory(&c->parent,sizeof(c->parent));s=WdfFdoQueryForInterface(device,&A04SharedInterfaceGuid,(PINTERFACE)&c->parent,sizeof(c->parent),A04_SHARED_INTERFACE_VERSION,NULL);
 KeysCheckpoint(KEYS_CP_INTERFACE,KEYS_CP_NTSTATUS,(ULONG)s,c->parent.Size,c->parent.Version,
  c->parent.Context!=NULL,c->parent.InterfaceReference!=NULL,c->parent.InterfaceDereference!=NULL,c->parent.ButtonPmicRead!=NULL);
 if(!NT_SUCCESS(s)){RtlZeroMemory(&c->parent,sizeof(c->parent));return s;}
 if(!a04_button_shared_interface_valid(&c->parent)){
  if(c->parent.Context&&c->parent.InterfaceDereference)c->parent.InterfaceDereference(c->parent.Context);
  RtlZeroMemory(&c->parent,sizeof(c->parent));return STATUS_REVISION_MISMATCH;}
 c->keypad=(PUCHAR)MmMapIoSpace(b->u.Memory.Start,b->u.Memory.Length,MmNonCached);
 if(!c->keypad){c->parent.InterfaceDereference(c->parent.Context);RtlZeroMemory(&c->parent,sizeof(c->parent));return STATUS_INSUFFICIENT_RESOURCES;}
 return STATUS_SUCCESS;
}
NTSTATUS KeysPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated) {
 NTSTATUS s;KeysCheckpoint(KEYS_CP_PREPARE,KEYS_CP_UNOBSERVED,0,WdfCmResourceListGetCount(raw),WdfCmResourceListGetCount(translated),0,0,0,0);
 s=KeysPrepareCore(device,raw,translated);KeysCheckpoint(KEYS_CP_PREPARE_RESULT,KEYS_CP_NTSTATUS,(ULONG)s,0,0,0,0,0,0);return s;
}
NTSTATUS KeysRelease(WDFDEVICE device,WDFCMRESLIST translated) {
 KEYS_CONTEXT *c=Keys(device);UNREFERENCED_PARAMETER(translated);
 if(c->keypad){MmUnmapIoSpace(c->keypad,0x1000);c->keypad=NULL;}
 if(c->parent.Context){c->parent.InterfaceDereference(c->parent.Context);RtlZeroMemory(&c->parent,sizeof(c->parent));}
 KeysCheckpoint(KEYS_CP_RELEASE,KEYS_CP_NTSTATUS,STATUS_SUCCESS,0,0,0,0,0,0);return STATUS_SUCCESS;
}
static USHORT KRead(KEYS_CONTEXT *c,ULONG off){return READ_REGISTER_USHORT((PUSHORT)(c->keypad+off));}
static VOID KWrite(KEYS_CONTEXT *c,ULONG off,USHORT v){WRITE_REGISTER_USHORT((PUSHORT)(c->keypad+off),v);}
static VOID ScanRestore(KEYS_CONTEXT *c){
 if(c->scan_owned){KWrite(c,0x24,c->saved_enable);KWrite(c,0x18,c->saved_debounce);c->scan_owned=FALSE;}
}
static int ScanStart(KEYS_CONTEXT *c){
 /* CT3 Linux hal_kpd: 16bit EN 0/1 and debounce14bit. Board DTS:1024.
  * XNU A04BR3Input::keyThread uses the same two writes and MEM1 read.
  * Reject bus-fill/unsupported state before writes. No clock/pinmux guessing.
  * Readback confirms programming only, never proves the scanner advances. */
 if(!c->keypad){c->scan_reason=1;return 0;}
 c->saved_enable=KRead(c,0x24);c->saved_debounce=KRead(c,0x18);
 if(c->saved_enable>1||(c->saved_debounce&0xc000)){c->scan_reason=2;return 0;}
 c->scan_owned=TRUE;KWrite(c,0x18,1024);KWrite(c,0x24,1);
 if(KRead(c,0x18)!=1024||KRead(c,0x24)!=1){c->scan_reason=3;ScanRestore(c);return 0;}c->scan_reason=0;return 1;
}
NTSTATUS KeysD0(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous) {
 KEYS_CONTEXT *c=Keys(device);int scan;UNREFERENCED_PARAMETER(previous);
 KeysCheckpoint(KEYS_CP_D0,KEYS_CP_UNOBSERVED,0,previous,c->parent.Context!=NULL,0,0,0,0);
 if(!c->parent.Context){KeysCheckpoint(KEYS_CP_D0_RESULT,KEYS_CP_NTSTATUS,STATUS_DEVICE_NOT_READY,0,0,0,0,0,0);return STATUS_DEVICE_NOT_READY;}
 WdfIoQueueStart(c->reads);WdfWaitLockAcquire(c->lock,NULL);mt8167_keypad_init(&c->state);c->pending=15;c->online=TRUE;
 scan=ScanStart(c);if(!scan){c->state.keypad.failed=1;DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT8167KEYPAD scan programming unavailable; PMIC independent\n");}WdfWaitLockRelease(c->lock);
 KeysCheckpoint(KEYS_CP_SCAN,KEYS_CP_BOOLEAN,(ULONG)scan,c->scan_reason,c->saved_enable,c->saved_debounce,c->scan_owned,0,0);
 WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(20));KeysCheckpoint(KEYS_CP_D0_RESULT,KEYS_CP_NTSTATUS,STATUS_SUCCESS,0,0,0,0,0,0);return STATUS_SUCCESS;
}
NTSTATUS KeysExit(WDFDEVICE device,WDF_POWER_DEVICE_STATE next) {
 KEYS_CONTEXT *c=Keys(device);UNREFERENCED_PARAMETER(next);
 WdfWaitLockAcquire(c->lock,NULL);c->online=FALSE;WdfWaitLockRelease(c->lock);
 WdfTimerStop(c->timer,TRUE);WdfTimerStop(c->timer,FALSE);
 WdfWaitLockAcquire(c->lock,NULL);ScanRestore(c);c->state.pmic.value=c->state.keypad.value=0;c->pending=15;WdfWaitLockRelease(c->lock);
 Drain(c);WdfIoQueuePurgeSynchronously(c->reads);KeysCheckpoint(KEYS_CP_D0_EXIT,KEYS_CP_NTSTATUS,STATUS_SUCCESS,next,0,0,0,0,0);return STATUS_SUCCESS;
}
VOID KeysTimer(WDFTIMER timer) {
 KEYS_CONTEXT *c=Keys((WDFDEVICE)WdfTimerGetParentObject(timer));USHORT raw=0;int rearm=0,valid,pmic_result=0,pmic_first_failure=0,keypad_first_failure=0;
 WdfWaitLockAcquire(c->lock,NULL);
 if(c->online){
  USHORT keypad=0xffff;int kpvalid=0;
  valid=0;if(!c->state.pmic.failed){pmic_result=c->parent.ButtonPmicRead(c->parent.Context,&raw);valid=pmic_result==0;}
  if(!c->state.keypad.failed&&c->scan_owned){
   kpvalid=KRead(c,0x24)==1&&KRead(c,0x18)==1024;
   if(kpvalid)keypad=KRead(c,0x04);
  }
  {
   int pf=c->state.pmic.failed,kf=c->state.keypad.failed;
   if(mt8167_keypad_sample(&c->state,valid,raw,kpvalid,keypad))c->pending=15;
   pmic_first_failure=!pf&&c->state.pmic.failed;keypad_first_failure=!kf&&c->state.keypad.failed;
   if(!pf&&c->state.pmic.failed)DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT8167KEYPAD PMIC supplier failure; PMIC reports released/stopped\n");
   if(!kf&&c->state.keypad.failed)DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT8167KEYPAD scan settings changed; keypad reports released/stopped\n");
  }
  rearm=!(c->state.pmic.failed&&c->state.keypad.failed);
 }
 WdfWaitLockRelease(c->lock);
 if(pmic_first_failure)KeysCheckpoint(KEYS_CP_PMIC_FAILURE,KEYS_CP_BACKEND,(ULONG)pmic_result,0,0,0,0,0,0);
 if(keypad_first_failure)KeysCheckpoint(KEYS_CP_KEYPAD_FAILURE,KEYS_CP_BOOLEAN,0,0,0,0,0,0,0);
 Drain(c);
 /* D0Exit closes online under this lock, then synchronously cancels the timer. */
 WdfWaitLockAcquire(c->lock,NULL);if(c->online&&rearm)WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(20));WdfWaitLockRelease(c->lock);
}
VOID KeysIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code) {
 KEYS_CONTEXT *c=Keys(WdfIoQueueGetDevice(queue));HID_DESCRIPTOR hid={0};HID_DEVICE_ATTRIBUTES attr={0};NTSTATUS s=STATUS_NOT_SUPPORTED;ULONG diag_stage=0;
 UNREFERENCED_PARAMETER(inlen);UNREFERENCED_PARAMETER(outlen);
 switch(code){
 case IOCTL_HID_GET_DEVICE_DESCRIPTOR:diag_stage=KEYS_CP_HID_DESCRIPTOR;
  hid.bLength=sizeof(hid);hid.bDescriptorType=HID_HID_DESCRIPTOR_TYPE;hid.bcdHID=0x0111;hid.bNumDescriptors=1;
  hid.DescriptorList[0].bReportType=HID_REPORT_DESCRIPTOR_TYPE;hid.DescriptorList[0].wReportLength=sizeof(mt8167_keypad_descriptor);s=Copy(request,&hid,sizeof(hid));break;
 case IOCTL_HID_GET_REPORT_DESCRIPTOR:diag_stage=KEYS_CP_REPORT_DESCRIPTOR;s=Copy(request,mt8167_keypad_descriptor,sizeof(mt8167_keypad_descriptor));break;
 case IOCTL_HID_GET_DEVICE_ATTRIBUTES:diag_stage=KEYS_CP_ATTRIBUTES;
  /* Software-enumerated parent child: no invented USB VID/PID identity. */
  attr.Size=sizeof(attr);attr.VersionNumber=1;s=Copy(request,&attr,sizeof(attr));break;
 case IOCTL_HID_READ_REPORT:diag_stage=KEYS_CP_READ_REQUEST;
  WdfWaitLockAcquire(c->lock,NULL);s=c->online?WdfRequestForwardToIoQueue(request,c->reads):STATUS_DEVICE_NOT_READY;WdfWaitLockRelease(c->lock);
  if(NT_SUCCESS(s)){KeysCheckpoint(KEYS_CP_READ_REQUEST,KEYS_CP_NTSTATUS,(ULONG)s,code,(ULONG)outlen,(ULONG)inlen,0,0,0);Drain(c);return;}KeysCheckpoint(KEYS_CP_FORWARD_FAILURE,KEYS_CP_NTSTATUS,(ULONG)s,0,0,0,0,0,0);break;
 case IOCTL_HID_GET_INPUT_REPORT:{PHID_XFER_PACKET p=(PHID_XFER_PACKET)WdfRequestWdmGetIrp(request)->UserBuffer;UCHAR report[2];
  diag_stage=KEYS_CP_GET_INPUT;
  if(outlen<sizeof(*p)||!p||!p->reportBuffer||p->reportBufferLen<2||(p->reportId<1||p->reportId>4)){s=STATUS_INVALID_PARAMETER;break;}
  WdfWaitLockAcquire(c->lock,NULL);mt8167_keypad_report(&c->state,p->reportId,report);WdfWaitLockRelease(c->lock);
  RtlCopyMemory(p->reportBuffer,report,2);WdfRequestSetInformation(request,2);s=STATUS_SUCCESS;break;}
 }
 if(diag_stage)KeysCheckpoint(diag_stage,KEYS_CP_NTSTATUS,(ULONG)s,code,(ULONG)outlen,(ULONG)inlen,0,0,0);
 WdfRequestComplete(request,s);
}
static NTSTATUS KeysAddCore(WDFDRIVER driver,PWDFDEVICE_INIT init) {
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
NTSTATUS KeysAdd(WDFDRIVER driver,PWDFDEVICE_INIT init){
 NTSTATUS s;KeysCheckpoint(KEYS_CP_ADD,KEYS_CP_UNOBSERVED,0,0,0,0,0,0,0);
 s=KeysAddCore(driver,init);KeysCheckpoint(KEYS_CP_ADD_RESULT,KEYS_CP_NTSTATUS,(ULONG)s,0,0,0,0,0,0);return s;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path){
 WDF_DRIVER_CONFIG c;NTSTATUS s;KeysCheckpointBegin(path);KeysCheckpoint(KEYS_CP_ENTRY,KEYS_CP_UNOBSERVED,0,0,0,0,0,0,0);
 WDF_DRIVER_CONFIG_INIT(&c,KeysAdd);s=WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE);
 KeysCheckpoint(KEYS_CP_DRIVER_RESULT,KEYS_CP_NTSTATUS,(ULONG)s,0,0,0,0,0,0);return s;
}
