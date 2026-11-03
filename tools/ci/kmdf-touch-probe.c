/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Build-only KMDF ABI probe. It does not register an I/O queue or touch
 * hardware; any attempt to add a device is deliberately rejected. */
#include <ntddk.h>
#include <wdf.h>

EVT_WDF_DRIVER_DEVICE_ADD A04ProbeDeviceAdd;

NTSTATUS A04ProbeDeviceAdd(WDFDRIVER driver, PWDFDEVICE_INIT device_init)
{
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(device_init);
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, A04ProbeDeviceAdd);
    return WdfDriverCreate(driver, registry_path, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}
