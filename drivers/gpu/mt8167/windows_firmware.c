/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Windows-side firmware boot composition. This is not a PnP or WDDM driver. */
#include "windows_firmware.h"
#include "pvr_rogue_fwif.h"
#define BOOT_TIMEOUT_STEPS 1000u
static NTSTATUS fw_status(enum a04_pvr_fw_result r)
{
    return r==A04_PVR_FW_INCOMPATIBLE ? STATUS_NOT_SUPPORTED : STATUS_INVALID_IMAGE_FORMAT;
}
NTSTATUS a04_gpu_firmware_boot(struct a04_gpu_firmware_state *s,
        PDMA_ADAPTER adapter, volatile void *regs, ULONG regs_length,
        const uint8_t *image, size_t image_size, uint64_t bvnc,
        BOOLEAN allow_mismatch, ULONG clock_hz)
{
    NTSTATUS status; enum a04_pvr_fw_result r; unsigned i; LARGE_INTEGER interval;
    volatile struct rogue_fwif_sysinit *sys;
    if (!s || !adapter || !image || !clock_hz || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        s->memory.adapter || s->processor_released) return STATUS_INVALID_PARAMETER;
    if (bvnc!=A04_PVR_BVNC) return STATUS_NOT_SUPPORTED;
    r=a04_pvr_fw_plan(image,image_size,bvnc,allow_mismatch,&s->plan);
    if (r!=A04_PVR_FW_OK) return fw_status(r);
    status=a04_gpu_register_io_init(&s->registers,regs,regs_length,&s->io);
    if (!NT_SUCCESS(status)) return status;
    status=a04_gpu_allocate_memory(adapter,&s->memory);
    if (!NT_SUCCESS(status)) return status;
    r=a04_pvr_fw_load(image,image_size,bvnc,allow_mismatch,s->memory.heap,
                      A04_PVR_HEAP_SIZE,&s->plan);
    if (r!=A04_PVR_FW_OK) { status=fw_status(r); goto fail_before_release; }
    r=a04_pvr_mips_prepare(&s->plan,s->memory.heap,A04_PVR_HEAP_SIZE,
        (uint64_t)s->memory.heap_logical.QuadPart,s->memory.table,0x4000u,
        (uint64_t)s->memory.table_logical.QuadPart,&s->remap);
    if (r!=A04_PVR_FW_OK) { status=STATUS_DEVICE_CONFIGURATION_ERROR; goto fail_before_release; }
    r=a04_pvr_runtime_prepare(&s->plan,s->memory.heap,A04_PVR_HEAP_SIZE,
        (uint64_t)s->memory.heap_logical.QuadPart,clock_hz,&s->runtime);
    if (r!=A04_PVR_FW_OK) { status=STATUS_DEVICE_CONFIGURATION_ERROR; goto fail_before_release; }
    a04_gpu_dma_barrier();
    /* Mark the allocation as owned by the GPU before the first MMIO write. */
    s->processor_released=TRUE;
    r=a04_pvr_mips_start(&s->io,bvnc,&s->remap);
    if (r!=A04_PVR_FW_OK) return STATUS_DEVICE_CONFIGURATION_ERROR;
    sys=(volatile struct rogue_fwif_sysinit *)(void *)((uint8_t *)s->memory.heap+s->runtime.system_init);
    interval.QuadPart=-100000; /* 10 ms, relative */
    for (i=0;i<BOOT_TIMEOUT_STEPS;++i) {
        a04_gpu_dma_barrier();
        if (sys->firmware_started) { s->firmware_started=TRUE; return STATUS_SUCCESS; }
        status=KeDelayExecutionThread(KernelMode,FALSE,&interval);
        if (!NT_SUCCESS(status)) return status;
    }
    return STATUS_IO_TIMEOUT;
fail_before_release:
    a04_gpu_free_memory(&s->memory);
    return status;
}
