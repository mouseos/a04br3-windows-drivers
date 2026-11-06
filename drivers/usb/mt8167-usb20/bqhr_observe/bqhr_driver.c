/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read BQHR on the MOU0008 ACPI stack only. Never map or access I2C.
 */
#include <ntddk.h>
#include <wdf.h>
#include "bqhr_ioctl.h"
#include "../../../power/bq24160/bqhr_acpi.h"
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD BqhrAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BqhrIoctl;
VOID BqhrIoctl(WDFQUEUE q,WDFREQUEST req,size_t outlen,size_t inlen,ULONG code)
{
 A04_BQHR_RESPONSE *out;size_t len;NTSTATUS status,io=STATUS_UNSUCCESSFUL;
 if(code!=A04_BQHR_GET || inlen){WdfRequestComplete(req,STATUS_INVALID_PARAMETER);return;}
 if(outlen<sizeof(*out)){WdfRequestComplete(req,STATUS_BUFFER_TOO_SMALL);return;}
 status=WdfRequestRetrieveOutputBuffer(req,sizeof(*out),(PVOID *)&out,&len);
 if(!NT_SUCCESS(status)){WdfRequestComplete(req,status);return;}
 RtlZeroMemory(out,sizeof(*out));out->size=sizeof(*out);out->version=1;
 out->result=(ULONG)a04_bqhr_receive(WdfIoQueueGetDevice(q),&io,out->record);
 out->status=(ULONG)io;
 WdfRequestCompleteWithInformation(req,STATUS_SUCCESS,sizeof(*out));
}
NTSTATUS BqhrAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_OBJECT_ATTRIBUTES attr;WDF_IO_QUEUE_CONFIG q;WDFDEVICE dev;NTSTATUS status;
 DECLARE_CONST_UNICODE_STRING(name,A04_BQHR_DEVICE);
 DECLARE_CONST_UNICODE_STRING(link,A04_BQHR_LINK);
 DECLARE_CONST_UNICODE_STRING(sddl,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
 UNREFERENCED_PARAMETER(driver);
 WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN,TRUE);
 status=WdfDeviceInitAssignName(init,&name);if(!NT_SUCCESS(status))return status;
 status=WdfDeviceInitAssignSDDLString(init,&sddl);if(!NT_SUCCESS(status))return status;
 WDF_OBJECT_ATTRIBUTES_INIT(&attr);attr.ExecutionLevel=WdfExecutionLevelPassive;
 status=WdfDeviceCreate(&init,&attr,&dev);if(!NT_SUCCESS(status))return status;
 status=WdfDeviceCreateSymbolicLink(dev,&link);if(!NT_SUCCESS(status))return status;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchSequential);
 q.PowerManaged=WdfTrue;q.EvtIoDeviceControl=BqhrIoctl;
 return WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{
 WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,BqhrAdd);
 return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
