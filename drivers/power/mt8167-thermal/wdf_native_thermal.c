/* SPDX-License-Identifier: GPL-2.0-only
 * Rejection-only native binding. The former backend admitted unqualified
 * hardware and depended on bugcheck after uncertain ADC/controller stop.
 * No mapping, supplier interface, lease, IRQ, DMA or readout is created here.
 * Returning failure from ReleaseHardware cannot guarantee WDF object lifetime;
 * admission must therefore fail BEFORE acquiring any hardware ownership.
 */
#include <ntddk.h>
#include <wdf.h>
#include "resources.h"
EVT_WDF_DRIVER_DEVICE_ADD NativeAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE NativePrepare;
EVT_WDF_DEVICE_D0_ENTRY NativeD0;
NTSTATUS NativePrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 struct a04_thermal_resource r[2];ULONG i;
 UNREFERENCED_PARAMETER(device);
 if(WdfCmResourceListGetCount(raw)!=2 || WdfCmResourceListGetCount(translated)!=2)
  return STATUS_DEVICE_CONFIGURATION_ERROR;
 RtlZeroMemory(r,sizeof(r));
 for(i=0;i<2;i++) {
  PCM_PARTIAL_RESOURCE_DESCRIPTOR x=WdfCmResourceListGetDescriptor(raw,i);
  PCM_PARTIAL_RESOURCE_DESCRIPTOR y=WdfCmResourceListGetDescriptor(translated,i);
  if(!x||!y||x->Type!=CmResourceTypeMemory||y->Type!=CmResourceTypeMemory)
   return STATUS_DEVICE_CONFIGURATION_ERROR;
  r[i].raw_memory=r[i].memory=1;
  r[i].raw_exclusive=x->ShareDisposition==CmResourceShareDeviceExclusive;
  r[i].exclusive=y->ShareDisposition==CmResourceShareDeviceExclusive;
  r[i].raw=x->u.Memory.Start.QuadPart;r[i].translated=y->u.Memory.Start.QuadPart;
  r[i].raw_size=x->u.Memory.Length;r[i].size=y->u.Memory.Length;
 }
 if(a04_thermal_resources_validate_shared(r,2))return STATUS_DEVICE_CONFIGURATION_ERROR;
 /* MMIO resource exclusivity and a native parent token prove neither
  * firmware/EL3 exclusion nor interrupt/fabric drain or removal retention.
  * There is no qualified implementation of that contract in this source.
  * Do not invent an enable flag or accept boot observation as qualification. */
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,
  "A04THM UNQUALIFIED no-claim no-MMIO no-reading\n");
 return STATUS_NOT_SUPPORTED; /* real PnP start failure, normally Code 10 */
}
NTSTATUS NativeD0(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
 UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(previous);
 return STATUS_NOT_SUPPORTED; /* defensive: never activate after failed admission */
}
NTSTATUS NativeAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDFDEVICE device;WDF_PNPPOWER_EVENT_CALLBACKS p;WDF_OBJECT_ATTRIBUTES a;
 UNREFERENCED_PARAMETER(driver);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);
 p.EvtDevicePrepareHardware=NativePrepare;p.EvtDeviceD0Entry=NativeD0;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ExecutionLevel=WdfExecutionLevelPassive;
 /* No temperature interface/queue is published for an unsupported device. */
 return WdfDeviceCreate(&init,&a,&device);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{
 WDF_DRIVER_CONFIG c;WDF_DRIVER_CONFIG_INIT(&c,NativeAdd);
 return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE);
}
