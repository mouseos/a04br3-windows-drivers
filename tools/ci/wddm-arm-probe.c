/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Build/link-only WDDM interface probe. Never registers or starts hardware. */
#include <ntddk.h>
#include <dispmprt.h>
#include "windows_firmware.h"
static volatile ULONG init_size;
static NTSTATUS (*volatile initialize_pointer)(PDRIVER_OBJECT, PUNICODE_STRING,
                                                PDRIVER_INITIALIZATION_DATA) = DxgkInitialize;
static NTSTATUS (*volatile boot_pointer)(struct a04_gpu_firmware_state *, PDMA_ADAPTER,
    volatile void *, ULONG, const uint8_t *, size_t, uint64_t, BOOLEAN, ULONG) = a04_gpu_firmware_boot;
static NTSTATUS (*volatile stop_pointer)(struct a04_gpu_firmware_state *, BOOLEAN) = a04_gpu_firmware_stop;
static NTSTATUS (*volatile health_pointer)(struct a04_gpu_firmware_state *) = a04_gpu_firmware_health;
static NTSTATUS (*volatile shutdown_pointer)(struct a04_gpu_firmware_state *) = a04_gpu_firmware_shutdown;
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    UNREFERENCED_PARAMETER(driver); UNREFERENCED_PARAMETER(path);
    init_size = sizeof(DRIVER_INITIALIZATION_DATA);
    if (!initialize_pointer || !boot_pointer || !stop_pointer || !health_pointer || !shutdown_pointer) return STATUS_UNSUCCESSFUL;
    return STATUS_NOT_SUPPORTED;
}
