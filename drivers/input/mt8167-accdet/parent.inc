/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "ioctl.h"
static int ParentAccdetRead32(void *context,unsigned off,uint32_t *value)
{return ParentRegRead(context,A04_SHARED_GPIO,off,value);}
static VOID ParentAccdetControl(WDFQUEUE queue,WDFREQUEST request,size_t outBytes,size_t inBytes,ULONG code)
{
 A04_PARENT_CONTEXT *c=ParentContext(WdfIoQueueGetDevice(queue));struct mt8167_accdet_socket *s=NULL;
 NTSTATUS status;enum mt8167_accdet_result result;struct mt8167_accdet_io io={c,ParentAccdetRead32};
 UNREFERENCED_PARAMETER(outBytes);
 if(code!=IOCTL_MT8167_ACCDET_READ_SOCKET||inBytes){WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return;}
 status=WdfRequestRetrieveOutputBuffer(request,sizeof(*s),(PVOID *)&s,NULL);
 if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
 RtlZeroMemory(s,sizeof(*s));
 if(ParentLock(c)){WdfRequestComplete(request,STATUS_DEVICE_NOT_READY);return;}
 if(!c->gpio||!c->regs.online||c->regs.poisoned||InterlockedCompareExchange(&c->removed,0,0)){ParentUnlock(c);WdfRequestComplete(request,STATUS_DEVICE_NOT_READY);return;}
 result=mt8167_accdet_read_socket(&io,s);ParentUnlock(c);
 status=result==MT8167_ACCDET_OK?STATUS_SUCCESS:result==MT8167_ACCDET_CHANGED?STATUS_DEVICE_BUSY:STATUS_DEVICE_NOT_READY;
 WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status)?sizeof(*s):0);
}
static NTSTATUS ParentAccdetPublish(WDFDEVICE device)
{
 WDF_IO_QUEUE_CONFIG config;WDF_OBJECT_ATTRIBUTES attributes;NTSTATUS status;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&config,WdfIoQueueDispatchSequential);
 config.PowerManaged=WdfTrue;config.EvtIoDeviceControl=ParentAccdetControl;
 WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ExecutionLevel=WdfExecutionLevelPassive;
 status=WdfIoQueueCreate(device,&config,&attributes,WDF_NO_HANDLE);if(!NT_SUCCESS(status))return status;
 return WdfDeviceCreateDeviceInterface(device,&GUID_MT8167_ACCDET_SOCKET,NULL);
}
