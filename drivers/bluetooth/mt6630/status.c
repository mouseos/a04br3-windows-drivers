/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed control device survives failed FDO D0Entry so failure remains readable.
 * Reads copy cached numeric metadata only; no FDO pointer, power-up, SDIO or HCI.
 */
#include "status.h"
static KSPIN_LOCK guard;
static MT6630_STATUS cached;
static WDFDEVICE control;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL StatusQuery;
void a04_bt_status_publish(const MT6630_STATUS *snapshot)
{
 KIRQL old;ULONG sequence;
 KeAcquireSpinLock(&guard,&old);sequence=cached.sequence+1;cached=*snapshot;
 cached.size=sizeof(cached);cached.version=MT6630_STATUS_VERSION;cached.sequence=sequence;
 KeReleaseSpinLock(&guard,old);
}
VOID StatusQuery(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 MT6630_STATUS *out;size_t bytes;KIRQL old;NTSTATUS status;
 UNREFERENCED_PARAMETER(queue);UNREFERENCED_PARAMETER(outlen);
 if(code!=MT6630_QUERY_STATUS||inlen){WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return;}
 status=WdfRequestRetrieveOutputBuffer(request,sizeof(*out),(PVOID *)&out,&bytes);
 if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
 KeAcquireSpinLock(&guard,&old);*out=cached;KeReleaseSpinLock(&guard,old);
 WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,sizeof(*out));
}
NTSTATUS a04_bt_status_create(WDFDRIVER driver)
{
 UNICODE_STRING acl=RTL_CONSTANT_STRING(L"D:P(A;;GA;;;SY)(A;;GR;;;BA)");
 UNICODE_STRING name=RTL_CONSTANT_STRING(L"\\Device\\MT6630BluetoothStatus");
 UNICODE_STRING link=RTL_CONSTANT_STRING(L"\\DosDevices\\MT6630BluetoothStatus");
 PWDFDEVICE_INIT init;WDF_IO_QUEUE_CONFIG q;WDF_OBJECT_ATTRIBUTES attr;NTSTATUS status;
 KeInitializeSpinLock(&guard);RtlZeroMemory(&cached,sizeof(cached));cached.size=sizeof(cached);
 cached.version=MT6630_STATUS_VERSION;cached.exclusive_bt_profile=1;
 cached.start_status=(ULONG)STATUS_DEVICE_NOT_READY;cached.read_version_status=MT6630_STATUS_UNKNOWN;
 init=WdfControlDeviceInitAllocate(driver,&acl);if(!init)return STATUS_INSUFFICIENT_RESOURCES;
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_UNKNOWN);WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN,FALSE);
 status=WdfDeviceInitAssignName(init,&name);if(!NT_SUCCESS(status))goto fail;
 WDF_OBJECT_ATTRIBUTES_INIT(&attr);attr.ExecutionLevel=WdfExecutionLevelPassive;
 status=WdfDeviceCreate(&init,&attr,&control);if(!NT_SUCCESS(status))goto fail;
 status=WdfDeviceCreateSymbolicLink(control,&link);if(!NT_SUCCESS(status))goto destroy;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchSequential);q.PowerManaged=WdfFalse;q.EvtIoDeviceControl=StatusQuery;
 status=WdfIoQueueCreate(control,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(status))goto destroy;
 WdfControlFinishInitializing(control);return STATUS_SUCCESS;
destroy:WdfObjectDelete(control);control=NULL;return status;
fail:WdfDeviceInitFree(init);return status;
}
void a04_bt_status_delete(void)
{if(control){WdfObjectDelete(control);control=NULL;}}
