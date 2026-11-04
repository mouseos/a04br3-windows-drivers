/* SPDX-License-Identifier: MIT
 * GE8300 NULL queue/fence experiment based on UniXNU NativePowerVRQueue.cpp. */
#include "windows_queue_test.h"
#include "firmware_objects.h"
#include "firmware_queue.h"
#include "gpu_mmu.h"
#define TEST_PAGES 6u
#define TEST_SIZE (TEST_PAGES*4096u)
NTSTATUS a04_gpu_queue_test_free(struct a04_gpu_firmware_state *s,struct a04_gpu_queue_test_memory *m)
{
    if (!s || !m) return STATUS_INVALID_PARAMETER;
    if (s->processor_released) return STATUS_DEVICE_BUSY;
    if (!m->adapter && !m->cpu) return STATUS_SUCCESS;
    if (!m->adapter || !m->cpu) return STATUS_INVALID_DEVICE_STATE;
    m->adapter->DmaOperations->FreeCommonBuffer(m->adapter,TEST_SIZE,m->logical,m->cpu,FALSE);
    m->cpu=0; m->adapter=0; m->logical.QuadPart=0;
    return STATUS_SUCCESS;
}
NTSTATUS a04_gpu_queue_test(struct a04_gpu_firmware_state *s,struct a04_gpu_queue_test_memory *m)
{
    struct a04_gpu_fw_arena arena={0}; struct a04_gpu_fw_object vm={0};
    struct a04_gpu_fw_queue q={0}; struct rogue_fwif_kccb_cmd kick={0};
    uint8_t *pages; uint64_t dma; size_t first; NTSTATUS status; LARGE_INTEGER interval; unsigned i;
    if (!s || !m || !s->processor_released || !s->firmware_started || s->queue_faulted ||
        !s->memory.adapter || !s->memory.heap || m->adapter || m->cpu || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    m->adapter=s->memory.adapter;
    m->cpu=m->adapter->DmaOperations->AllocateCommonBuffer(m->adapter,TEST_SIZE,&m->logical,FALSE);
    if (!m->cpu) { m->adapter=0; return STATUS_INSUFFICIENT_RESOURCES; }
    dma=(uint64_t)m->logical.QuadPart; pages=m->cpu;
    if (((uintptr_t)pages&4095u) || (dma&4095u) || dma>UINT64_C(0x100000000)-TEST_SIZE)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    RtlZeroMemory(pages,TEST_SIZE);
    if (a04_gpu_mmu_map_brn65273((uint32_t *)(void *)pages,(uint64_t *)(void *)(pages+4096),
        (uint64_t *)(void *)(pages+8192),(uint64_t *)(void *)(pages+12288),(uint64_t *)(void *)(pages+16384),
        dma+4096,dma+8192,dma+12288,dma+16384,dma+20480)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    first=(s->runtime.next_free+4095u)&~(size_t)4095u;
    if (a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,
            (uint64_t)s->memory.heap_logical.QuadPart,first) ||
        a04_gpu_fw_memory_context(&arena,dma,&vm) ||
        a04_gpu_fw_prepare_null_queue(&arena,&vm,1,1,&q,&kick)) return STATUS_INSUFFICIENT_RESOURCES;
#define SYNC_OBJECT(o) do { status=a04_gpu_dma_sync((o).cpu,(ULONG)(o).size,FALSE); if (!NT_SUCCESS(status)) return status; } while (0)
    status=a04_gpu_dma_sync(pages,TEST_SIZE,FALSE); if (!NT_SUCCESS(status)) return status;
    SYNC_OBJECT(vm); SYNC_OBJECT(q.control); SYNC_OBJECT(q.ring); SYNC_OBJECT(q.state);
    SYNC_OBJECT(q.fence); SYNC_OBJECT(q.context);
#undef SYNC_OBJECT
    status=a04_gpu_firmware_invalidate_mmu(s); if (!NT_SUCCESS(status)) return status;
    status=a04_gpu_firmware_submit_kick(s,&kick); if (!NT_SUCCESS(status)) return status;
    interval.QuadPart=-100000; /* 10 ms */
    for(i=0;i<200u;++i) {
        status=a04_gpu_dma_sync(q.fence.cpu,(ULONG)q.fence.size,TRUE);
        if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
        if (*(volatile uint32_t *)q.fence.cpu==1u) return STATUS_SUCCESS;
        status=KeDelayExecutionThread(KernelMode,FALSE,&interval);
        if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
    }
    s->queue_faulted=TRUE;
    return STATUS_IO_TIMEOUT;
}
