/* Diagnostic HID transport for one MT8167 USB boot keyboard. This is not a
 * general USB host driver. It is not packaged until a board owner connects
 * KbHidReport/KbHidDisconnected to a qualified MUSB controller lifetime. */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include "keyboard.h"
#define REPORT_SLOTS 32u
typedef struct {
 WDFWAITLOCK lock; WDFQUEUE reads; UCHAR reports[REPORT_SLOTS][8];
 unsigned head,count; BOOLEAN connected,overflow,draining;
} KB_HID_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(KB_HID_CONTEXT,KbHidContext)
EVT_WDF_DRIVER_DEVICE_ADD KbHidAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL KbHidIoctl;
static NTSTATUS CopyOutput(WDFREQUEST r,const void *p,size_t n)
{
 WDFMEMORY memory; size_t length; NTSTATUS status=WdfRequestRetrieveOutputMemory(r,&memory);
 if(!NT_SUCCESS(status))return status;
 (void)WdfMemoryGetBuffer(memory,&length); if(length<n)return STATUS_BUFFER_TOO_SMALL;
 status=WdfMemoryCopyFromBuffer(memory,0,(PVOID)p,n);
 if(NT_SUCCESS(status))WdfRequestSetInformation(r,n);
 return status;
}
static void Drain(WDFDEVICE dev)
{
 KB_HID_CONTEXT *c=KbHidContext(dev); WDFREQUEST r; UCHAR report[8]; NTSTATUS status;
 WdfWaitLockAcquire(c->lock,NULL);
 if(c->draining) { WdfWaitLockRelease(c->lock); return; }
 c->draining=TRUE; WdfWaitLockRelease(c->lock);
 for(;;) {
  WdfWaitLockAcquire(c->lock,NULL);
  if(!c->count || !NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->reads,&r))) {
   c->draining=FALSE; WdfWaitLockRelease(c->lock); return;
  }
  RtlCopyMemory(report,c->reports[c->head],8); c->head=(c->head+1u)%REPORT_SLOTS; --c->count;
  WdfWaitLockRelease(c->lock);
  status=CopyOutput(r,report,8); WdfRequestComplete(r,status);
 }
}
/* Called at PASSIVE_LEVEL after a complete eight-byte boot report. */
NTSTATUS KbHidReport(WDFDEVICE dev,const UCHAR report[8])
{
 KB_HID_CONTEXT *c=KbHidContext(dev); unsigned slot;
 if(!report)return STATUS_INVALID_PARAMETER;
 WdfWaitLockAcquire(c->lock,NULL);
 if(c->overflow) { WdfWaitLockRelease(c->lock); return STATUS_DEVICE_DATA_ERROR; }
 if(c->count==REPORT_SLOTS) { c->overflow=TRUE; WdfWaitLockRelease(c->lock); WdfIoQueuePurge(c->reads,WDF_NO_EVENT_CALLBACK,WDF_NO_CONTEXT); return STATUS_BUFFER_OVERFLOW; }
 slot=(c->head+c->count)%REPORT_SLOTS; RtlCopyMemory(c->reports[slot],report,8); ++c->count; c->connected=TRUE;
 WdfWaitLockRelease(c->lock); Drain(dev); return STATUS_SUCCESS;
}
/* On unplug queue a release report so a key cannot remain held. */
void KbHidDisconnected(WDFDEVICE dev)
{
 KB_HID_CONTEXT *c=KbHidContext(dev); static const UCHAR release[8]={0};
 WdfWaitLockAcquire(c->lock,NULL); c->head=0; c->count=0; c->overflow=FALSE; c->connected=FALSE;
 RtlCopyMemory(c->reports[0],release,8); c->count=1;
 WdfWaitLockRelease(c->lock); WdfIoQueueStart(c->reads); Drain(dev);
}
VOID KbHidIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 WDFDEVICE dev=WdfIoQueueGetDevice(queue); KB_HID_CONTEXT *c=KbHidContext(dev);
 HID_DESCRIPTOR h={0}; HID_DEVICE_ATTRIBUTES attr={0}; const unsigned char *desc; unsigned len; NTSTATUS status=STATUS_NOT_SUPPORTED;
 UNREFERENCED_PARAMETER(outlen); UNREFERENCED_PARAMETER(inlen);
 switch(code) {
 case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
  (void)KbHidDescriptor(&len); h.bLength=sizeof(h); h.bDescriptorType=HID_HID_DESCRIPTOR_TYPE;
  h.bcdHID=0x0111; h.bNumDescriptors=1; h.DescriptorList[0].bReportType=HID_REPORT_DESCRIPTOR_TYPE;
  h.DescriptorList[0].wReportLength=(USHORT)len; status=CopyOutput(request,&h,sizeof(h)); break;
 case IOCTL_HID_GET_REPORT_DESCRIPTOR: desc=KbHidDescriptor(&len); status=CopyOutput(request,desc,len); break;
 case IOCTL_HID_GET_DEVICE_ATTRIBUTES: attr.Size=sizeof(attr); attr.VersionNumber=1; status=CopyOutput(request,&attr,sizeof(attr)); break;
 case IOCTL_HID_READ_REPORT:
  status=WdfRequestForwardToIoQueue(request,c->reads); if(NT_SUCCESS(status)) { Drain(dev); return; } break;
 }
 WdfRequestComplete(request,status);
}
NTSTATUS KbHidAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_OBJECT_ATTRIBUTES attr,child; WDF_IO_QUEUE_CONFIG q; WDFDEVICE dev; KB_HID_CONTEXT *c; NTSTATUS s;
 UNREFERENCED_PARAMETER(driver); WdfFdoInitSetFilter(init);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,KB_HID_CONTEXT); attr.ExecutionLevel=WdfExecutionLevelPassive;
 s=WdfDeviceCreate(&init,&attr,&dev); if(!NT_SUCCESS(s))return s; c=KbHidContext(dev);
 WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject=dev; s=WdfWaitLockCreate(&child,&c->lock); if(!NT_SUCCESS(s))return s;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel); q.EvtIoInternalDeviceControl=KbHidIoctl;
 s=WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE); if(!NT_SUCCESS(s))return s;
 WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual); return WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->reads);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{ WDF_DRIVER_CONFIG c; WDF_DRIVER_CONFIG_INIT(&c,KbHidAdd); return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE); }
