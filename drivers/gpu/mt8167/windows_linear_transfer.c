/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_linear_transfer.h"
#define BUFFER_SIZE (A04_TRANSFER_PAGES*4096u)
static NTSTATUS poison(struct a04_windows_transfer_mapping *m,NTSTATUS status)
{
    m->mapping.state=A04_TRANSFER_POISONED;
    m->firmware->queue_faulted=TRUE;
    return status;
}
NTSTATUS a04_windows_linear_transfer(struct a04_windows_transfer_mapping *m,
    const struct rogue_fwif_cmd_transfer *command,uint32_t job,
    const uint64_t *source_pages,const uint64_t *dest_pages,
    void *source_cpu,void *dest_cpu,const struct a04_transfer_range *ranges,size_t count)
{
    NTSTATUS status; struct rogue_fwif_kccb_cmd kick;
    uintptr_t source=(uintptr_t)source_cpu,dest=(uintptr_t)dest_cpu;
    if (!m || !m->firmware || !m->queue || !command || !job || !source_cpu || !dest_cpu ||
        (source&4095u) || (dest&4095u) || source>UINTPTR_MAX-BUFFER_SIZE ||
        dest>UINTPTR_MAX-BUFFER_SIZE || (source<dest+BUFFER_SIZE && dest<source+BUFFER_SIZE) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || m->firmware->queue_faulted ||
        !m->firmware->processor_released || !m->firmware->firmware_started ||
        !m->completed_fence || m->completed_fence==UINT32_MAX)
        return STATUS_INVALID_PARAMETER;
    m->last_status=STATUS_SUCCESS;
    if (a04_transfer_map(&m->mapping,source_pages,dest_pages,ranges,count))
        return NT_SUCCESS(m->last_status) ? STATUS_DEVICE_CONFIGURATION_ERROR : m->last_status;
    status=a04_gpu_dma_sync(source_cpu,BUFFER_SIZE,FALSE);
    if (!NT_SUCCESS(status)) return poison(m,status);
    status=a04_gpu_dma_sync(dest_cpu,BUFFER_SIZE,FALSE);
    if (!NT_SUCCESS(status)) return poison(m,status);
    if (a04_gpu_fw_append_transfer(m->queue,command,job,m->completed_fence,&kick))
        return poison(m,STATUS_INVALID_DEVICE_STATE);
    status=a04_gpu_dma_sync(m->queue->ring.cpu,(ULONG)m->queue->ring.size,FALSE);
    if (!NT_SUCCESS(status)) return poison(m,status);
    status=a04_gpu_firmware_submit_kick(m->firmware,&kick);
    if (!NT_SUCCESS(status)) return poison(m,status);
    ++m->completed_fence;
    if (m->mapping.ops.wait_idle(m->mapping.context)) return poison(m,m->last_status);
    status=a04_gpu_dma_sync(dest_cpu,BUFFER_SIZE,TRUE);
    if (!NT_SUCCESS(status)) return poison(m,status);
    if (a04_transfer_restore(&m->mapping))
        return poison(m,NT_SUCCESS(m->last_status) ? STATUS_DEVICE_CONFIGURATION_ERROR : m->last_status);
    return STATUS_SUCCESS;
}
