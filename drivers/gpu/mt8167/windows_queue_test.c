/* SPDX-License-Identifier: MIT
 * GE8300 NULL queue/fence experiment based on UniXNU NativePowerVRQueue.cpp. */
#include "windows_queue_test.h"
#include "firmware_objects.h"
#include "firmware_queue.h"
#include "gpu_mmu.h"
#ifndef A04_GE8300_CLEAR_TEST
#define A04_GE8300_CLEAR_TEST 0
#endif
#ifndef A04_GE8300_COPY_TEST
#define A04_GE8300_COPY_TEST 0
#endif
#if A04_GE8300_CLEAR_TEST && A04_GE8300_COPY_TEST
#error Select only one GE8300 diagnostic
#endif
#if A04_GE8300_COPY_TEST
#include "copy_fixture.h"
#define TEST_PAGES (14u+A04_GPU_COPY_PAGES)
#elif A04_GE8300_CLEAR_TEST
#include "clear_fixture.h"
#define TEST_PAGES 18u
#else
#define TEST_PAGES 6u
#endif
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
#if A04_GE8300_CLEAR_TEST || A04_GE8300_COPY_TEST
    struct rogue_fwif_cmd_transfer transfer={0};
#endif
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
#if A04_GE8300_COPY_TEST
    /* Four distinct PC/PD groups for the fixed Mesa-generated VA set.
     * The arena contains only our own DMA pages; no caller data is mapped. */
    for (size_t region=0;region<a04_gpu_copy_region_count;++region) {
        const struct a04_gpu_copy_region *r=&a04_gpu_copy_regions[region];
        size_t pd=6u+2u*r->group,pt=pd+1u,first=14u+r->page_offset;
        if (r->group>=A04_GPU_COPY_GROUPS || first>TEST_PAGES ||
            r->page_count>TEST_PAGES-first ||
            a04_gpu_mmu_map_4k((uint32_t *)(void *)pages,(uint64_t *)(void *)(pages+pd*4096u),
                (uint64_t *)(void *)(pages+pt*4096u),dma+pd*4096u,dma+pt*4096u,
                r->va,dma+first*4096u,r->page_count,0)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    if (a04_gpu_copy_fixture(pages+14u*4096u,A04_GPU_COPY_PAGES*4096u,&transfer))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    {
        uint8_t *source=pages+(14u+A04_GPU_COPY_SOURCE_PAGE)*4096u;
        static const uint8_t pixel[4]={0x11,0x5a,0xa5,0xee};
        for (size_t i=0;i<4u*4096u;++i) source[i]=pixel[i&3u];
        RtlFillMemory(pages+(14u+A04_GPU_COPY_DEST_PAGE)*4096u,4u*4096u,0x5a);
    }
#elif A04_GE8300_CLEAR_TEST
    /* Isolated VA space: only the fixed command's PDS, USC and output pages.
     * Keep all tables and image live until firmware and GPU have stopped. */
    if (a04_gpu_mmu_map_4k((uint32_t *)(void *)pages,(uint64_t *)(void *)(pages+6u*4096u),
            (uint64_t *)(void *)(pages+7u*4096u),dma+6u*4096u,dma+7u*4096u,
            UINT64_C(0x80000b6000),dma+14u*4096u,4,0) ||
        a04_gpu_mmu_map_4k((uint32_t *)(void *)pages,(uint64_t *)(void *)(pages+8u*4096u),
            (uint64_t *)(void *)(pages+9u*4096u),dma+8u*4096u,dma+9u*4096u,
            UINT64_C(0xda00002000),dma+12u*4096u,1,A04_GPU_MMU_READ_ONLY) ||
        a04_gpu_mmu_map_4k((uint32_t *)(void *)pages,(uint64_t *)(void *)(pages+10u*4096u),
            (uint64_t *)(void *)(pages+11u*4096u),dma+10u*4096u,dma+11u*4096u,
            UINT64_C(0xe000005000),dma+13u*4096u,1,A04_GPU_MMU_READ_ONLY) ||
        a04_gpu_clear_fixture(pages+12u*4096u,pages+13u*4096u,&transfer))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    RtlFillMemory(pages+14u*4096u,4u*4096u,0x5a);
#endif
    first=(s->runtime.next_free+4095u)&~(size_t)4095u;
    if (a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,
            (uint64_t)s->memory.heap_logical.QuadPart,first) ||
        a04_gpu_fw_memory_context(&arena,dma,&vm) ||
#if A04_GE8300_CLEAR_TEST || A04_GE8300_COPY_TEST
        /* Vendor MT8167 BVNC table: NUM_ISP_IPP_PIPES index 3 = 4. */
        a04_gpu_fw_prepare_transfer_queue(&arena,&vm,1,1,4,&transfer,&q,&kick))
#else
        a04_gpu_fw_prepare_null_queue(&arena,&vm,1,1,&q,&kick))
#endif
        return STATUS_INSUFFICIENT_RESOURCES;
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
        if (*(volatile uint32_t *)q.fence.cpu==1u) {
#if A04_GE8300_CLEAR_TEST || A04_GE8300_COPY_TEST
#if A04_GE8300_COPY_TEST
            const uint8_t *image=pages+(14u+A04_GPU_COPY_DEST_PAGE)*4096u;
#else
            const uint8_t *image=pages+14u*4096u;
#endif
            static const uint8_t expected[4]={0x11,0x5a,0xa5,0xee};
            status=a04_gpu_dma_sync((void *)image,4u*4096u,TRUE);
            if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
            for (unsigned pixel=0;pixel<4096u;++pixel)
                for (unsigned component=0;component<4u;++component)
                    if (image[pixel*4u+component]!=expected[component])
                        return STATUS_DATA_ERROR;
#endif
            return STATUS_SUCCESS;
        }
        status=KeDelayExecutionThread(KernelMode,FALSE,&interval);
        if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
    }
    s->queue_faulted=TRUE;
    return STATUS_IO_TIMEOUT;
}
