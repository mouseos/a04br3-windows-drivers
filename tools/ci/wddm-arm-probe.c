/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Build/link-only WDDM interface probe. Never registers or starts hardware. */
#include <ntddk.h>
#include <dispmprt.h>
static volatile ULONG init_size;
static NTSTATUS (*volatile initialize_pointer)(PDRIVER_OBJECT, PUNICODE_STRING,
                                                PDRIVER_INITIALIZATION_DATA) = DxgkInitialize;
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    UNREFERENCED_PARAMETER(driver); UNREFERENCED_PARAMETER(path);
    init_size = sizeof(DRIVER_INITIALIZATION_DATA);
    if (!initialize_pointer) return STATUS_UNSUCCESSFUL;
    return STATUS_NOT_SUPPORTED;
}
