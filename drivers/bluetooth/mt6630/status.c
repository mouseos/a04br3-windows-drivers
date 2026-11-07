/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed control device survives failed FDO D0Entry so failure remains readable.
 * Reads copy cached numeric metadata only; no FDO pointer, power-up, SDIO or HCI.
 */
#include "status.h"
static KSPIN_LOCK guard;
static MT6630_STATUS cached;
static MT6630_BTHX_CONTRACT contract;
C_ASSERT(ScoSupportHCIBypass==MT6630_BTHX_REQUIRED_SCO);
static WDFDEVICE control;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL StatusQuery;
void a04_bt_status_publish(const MT6630_STATUS *snapshot)
{
 KIRQL old;ULONG sequence;
 KeAcquireSpinLock(&guard,&old);sequence=cached.sequence+1;cached=*snapshot;
 cached.size=sizeof(cached);cached.version=MT6630_STATUS_VERSION;cached.sequence=sequence;
 KeReleaseSpinLock(&guard,old);
}
/* The caller records the actual reply before completing QUERY_CAPABILITIES.
 * STATUS_SUCCESS for a metadata IOCTL is not Windows stack load success. */
void a04_bt_status_capabilities(const BTHX_CAPABILITIES *caps,NTSTATUS status)
{
 KIRQL old;KeAcquireSpinLock(&guard,&old);contract.queries++;
 contract.last_query_status=(ULONG)status;contract.returned_capabilities=NT_SUCCESS(status)&&caps;
 contract.advertised_sco=contract.advertised_channels=contract.contract_satisfied=MT6630_STATUS_UNKNOWN;
 if(contract.returned_capabilities){
  contract.advertised_sco=(ULONG)caps->ScoSupport;contract.advertised_channels=caps->MaxScoChannels;
  contract.contract_satisfied=caps->ScoSupport==ScoSupportHCIBypass&&caps->MaxScoChannels==MT6630_BTHX_REQUIRED_CHANNELS;
 }
 KeReleaseSpinLock(&guard,old);
}
VOID StatusQuery(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 PVOID out;size_t bytes;ULONG length;KIRQL old;NTSTATUS status;
 UNREFERENCED_PARAMETER(queue);UNREFERENCED_PARAMETER(outlen);
 if(inlen){WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return;}
 if(code==MT6630_QUERY_STATUS)length=sizeof(cached);
 else if(code==MT6630_QUERY_BTHX_CONTRACT)length=sizeof(contract);
 else {WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return;}
 status=WdfRequestRetrieveOutputBuffer(request,length,&out,&bytes);
 if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
 KeAcquireSpinLock(&guard,&old);
 if(code==MT6630_QUERY_STATUS)RtlCopyMemory(out,&cached,length);else RtlCopyMemory(out,&contract,length);
 KeReleaseSpinLock(&guard,old);WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,length);
}
NTSTATUS a04_bt_status_create(WDFDRIVER driver)
{
 UNICODE_STRING acl=RTL_CONSTANT_STRING(L"D:P(A;;GA;;;SY)(A;;GR;;;BA)");
 UNICODE_STRING name=RTL_CONSTANT_STRING(L"\\Device\\MT6630BluetoothStatus");
 UNICODE_STRING link=RTL_CONSTANT_STRING(L"\\DosDevices\\MT6630BluetoothStatus");
 PWDFDEVICE_INIT init;WDF_IO_QUEUE_CONFIG q;WDF_OBJECT_ATTRIBUTES attr;NTSTATUS status;
 KeInitializeSpinLock(&guard);RtlZeroMemory(&cached,sizeof(cached));cached.size=sizeof(cached);
 cached.version=MT6630_STATUS_VERSION;cached.exclusive_bt_profile=1;
 RtlZeroMemory(&contract,sizeof(contract));contract.size=sizeof(contract);contract.version=MT6630_BTHX_CONTRACT_VERSION;
 contract.required_sco=MT6630_BTHX_REQUIRED_SCO;contract.required_channels=MT6630_BTHX_REQUIRED_CHANNELS;
 contract.advertised_sco=contract.advertised_channels=contract.contract_satisfied=contract.last_query_status=MT6630_STATUS_UNKNOWN;
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
