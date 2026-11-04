/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_render_memory.h"
#if A04_GE8300_RENDER_EXPERIMENT
#include "gpu_mmu.h"
#define PAGE 4096u
#define TABLE_PAGES 14u
#define MAX_PAYLOAD_PAGES 4096u
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
NTSTATUS a04_windows_render_memory_create(struct a04_windows_render_memory *m,
    struct a04_gpu_firmware_state *s,ULONG payload_pages,
    const struct a04_windows_render_map *maps,size_t count)
{
    uint64_t dma; uint8_t *p; size_t i,j; ULONG total;
    if (!m || !s || !maps || !count || count>A04_RENDER_MAX_MAPS ||
        !payload_pages || payload_pages>MAX_PAYLOAD_PAGES || m->cpu || m->adapter || m->firmware ||
        !s->processor_released || !s->firmware_started || s->queue_faulted ||
        !s->memory.heap || !s->memory.adapter || !s->memory.adapter->DmaOperations ||
        !s->memory.adapter->DmaOperations->AllocateCommonBuffer ||
        !s->memory.adapter->DmaOperations->FreeCommonBuffer || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    for (i=0;i<count;++i) {
        const struct a04_windows_render_map *a=&maps[i];
        if (!a->pages || a->group>=4 || a->payload_page>=payload_pages ||
            a->pages>payload_pages-a->payload_page || (a->gpu_va&(PAGE-1)) ||
            !a->gpu_va || a->gpu_va>=UINT64_C(1)<<40 ||
            (uint64_t)a->pages*PAGE>(UINT64_C(1)<<40)-a->gpu_va ||
            (a->mmu_flags & ~(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_COHERENT|A04_GPU_MMU_SLC_BYPASS)))
            return STATUS_INVALID_PARAMETER;
        for (j=0;j<i;++j) {
            const struct a04_windows_render_map *b=&maps[j];
            if (overlap(a->gpu_va,(uint64_t)a->pages*PAGE,b->gpu_va,(uint64_t)b->pages*PAGE) ||
                overlap(a->payload_page,a->pages,b->payload_page,b->pages)) return STATUS_INVALID_PARAMETER;
        }
    }
    total=(TABLE_PAGES+payload_pages)*PAGE;
    m->firmware=s;m->adapter=s->memory.adapter;m->length=total;
    m->cpu=m->adapter->DmaOperations->AllocateCommonBuffer(m->adapter,total,&m->logical,FALSE);
    if (!m->cpu) { m->firmware=0;m->adapter=0;m->length=0;return STATUS_INSUFFICIENT_RESOURCES; }
    dma=(uint64_t)m->logical.QuadPart;p=m->cpu;
    if (((uintptr_t)p&(PAGE-1)) || (dma&(PAGE-1)) || dma>UINT64_C(0x100000000)-total ||
        overlap(dma,total,(uint64_t)s->memory.heap_logical.QuadPart,A04_PVR_HEAP_SIZE) ||
        (s->memory.table && overlap(dma,total,(uint64_t)s->memory.table_logical.QuadPart,0x4000u)))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    RtlZeroMemory(p,total);
    if (a04_gpu_mmu_map_brn65273((uint32_t *)(void *)p,(uint64_t *)(void *)(p+PAGE),
        (uint64_t *)(void *)(p+2u*PAGE),(uint64_t *)(void *)(p+3u*PAGE),
        (uint64_t *)(void *)(p+4u*PAGE),dma+PAGE,dma+2u*PAGE,dma+3u*PAGE,dma+4u*PAGE,dma+5u*PAGE))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    for (i=0;i<count;++i) {
        const struct a04_windows_render_map *r=&maps[i];size_t pd=6u+2u*r->group,pt=pd+1u;
        if (a04_gpu_mmu_map_4k((uint32_t *)(void *)p,(uint64_t *)(void *)(p+pd*PAGE),
            (uint64_t *)(void *)(p+pt*PAGE),dma+pd*PAGE,dma+pt*PAGE,r->gpu_va,
            dma+(TABLE_PAGES+r->payload_page)*PAGE,r->pages,r->mmu_flags))
            return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    m->ready=TRUE;
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_memory_publish(struct a04_windows_render_memory *m)
{
    NTSTATUS status;
    if (!m || !m->firmware || !m->cpu || !m->adapter || !m->ready || m->published || m->poisoned ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    m->poisoned=TRUE;
    status=a04_gpu_dma_sync(m->cpu,m->length,FALSE);
    if (!NT_SUCCESS(status)) return status;
    status=a04_gpu_firmware_invalidate_mmu(m->firmware);
    if (!NT_SUCCESS(status)) return status;
    m->published=TRUE;m->poisoned=FALSE;return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_memory_free(struct a04_windows_render_memory *m, BOOLEAN gpu_dma_quiesced)
{
    if (!m || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    if (!m->firmware && !m->adapter && !m->cpu) return STATUS_SUCCESS;
    if (!m->firmware || !m->adapter || !m->cpu || !m->length) return STATUS_INVALID_DEVICE_STATE;
    if (m->firmware->processor_released || !gpu_dma_quiesced) return STATUS_DEVICE_BUSY;
    m->adapter->DmaOperations->FreeCommonBuffer(m->adapter,m->length,m->logical,m->cpu,FALSE);
    RtlZeroMemory(m,sizeof(*m));return STATUS_SUCCESS;
}
#endif
