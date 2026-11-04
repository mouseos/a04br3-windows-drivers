/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_linear_context.h"
#include "gpu_mmu.h"
#define PAGE 4096u
#define PAGES (14u+A04_GPU_LINEAR_COPY_PAGES+12u)
#define SIZE (PAGES*PAGE)
#define PAYLOAD (4u*PAGE)

static int overlaps(uintptr_t a,size_t alen,uintptr_t b,size_t blen)
{ return a<=b ? b-a<alen : a-b<blen; }

NTSTATUS a04_windows_linear_context_copy(struct a04_windows_linear_context *c,
                                         const void *input,void *output)
{
    struct a04_gpu_firmware_state *s; struct a04_transfer_range ranges[3];
    uint64_t src[6],dst[6],dma; uint8_t *source,*dest; size_t i,n=2; NTSTATUS status;
    uintptr_t a=(uintptr_t)input,b=(uintptr_t)output,base,heap;
    if (!c || !input || !output || !c->firmware || !c->memory.cpu ||
        !c->memory.adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        c->mapping.mapping.state!=A04_TRANSFER_SCRATCH || !c->mapping.completed_fence ||
        c->mapping.completed_fence==UINT32_MAX || a>UINTPTR_MAX-PAYLOAD || b>UINTPTR_MAX-PAYLOAD)
        return STATUS_INVALID_PARAMETER;
    s=c->firmware; base=(uintptr_t)c->memory.cpu; heap=(uintptr_t)s->memory.heap;
    if (!heap || base>UINTPTR_MAX-SIZE || heap>UINTPTR_MAX-A04_PVR_HEAP_SIZE ||
        overlaps(a,PAYLOAD,base,SIZE) || overlaps(b,PAYLOAD,base,SIZE) ||
        overlaps(a,PAYLOAD,heap,A04_PVR_HEAP_SIZE) || overlaps(b,PAYLOAD,heap,A04_PVR_HEAP_SIZE))
        return STATUS_INVALID_PARAMETER;
    if (s->memory.table && ((uintptr_t)s->memory.table>UINTPTR_MAX-0x4000u ||
        overlaps(a,PAYLOAD,(uintptr_t)s->memory.table,0x4000u) ||
        overlaps(b,PAYLOAD,(uintptr_t)s->memory.table,0x4000u))) return STATUS_INVALID_PARAMETER;
    dma=(uint64_t)c->memory.logical.QuadPart;
    source=(uint8_t *)c->memory.cpu+(14u+A04_GPU_LINEAR_COPY_PAGES)*PAGE;
    dest=(uint8_t *)c->memory.cpu+(20u+A04_GPU_LINEAR_COPY_PAGES)*PAGE;
    for (i=0;i<6u;++i) {
        src[i]=dma+(14u+A04_GPU_LINEAR_COPY_PAGES+i)*PAGE;
        dst[i]=dma+(20u+A04_GPU_LINEAR_COPY_PAGES+i)*PAGE;
    }
    ranges[0].address=dma; ranges[0].length=(14u+A04_GPU_LINEAR_COPY_PAGES)*PAGE;
    ranges[1].address=(uint64_t)s->memory.heap_logical.QuadPart; ranges[1].length=A04_PVR_HEAP_SIZE;
    if (s->memory.table) {
        ranges[2].address=(uint64_t)s->memory.table_logical.QuadPart; ranges[2].length=0x4000u; n=3;
    }
    RtlCopyMemory(source+PAGE,input,PAYLOAD);
    status=a04_windows_linear_transfer(&c->mapping,&c->command,c->mapping.completed_fence+1u,
                                       src,dst,source,dest,ranges,n);
    if (!NT_SUCCESS(status)) return status;
    RtlCopyMemory(output,dest+PAGE,PAYLOAD); return STATUS_SUCCESS;
}

NTSTATUS a04_windows_linear_context_free(struct a04_windows_linear_context *c,
                                        struct a04_gpu_firmware_state *s)
{
    if (!c || !s || (c->firmware && c->firmware!=s)) return STATUS_INVALID_PARAMETER;
    if (s->processor_released) return STATUS_DEVICE_BUSY;
    if (!c->memory.cpu && !c->memory.adapter) return STATUS_SUCCESS;
    if (!c->memory.cpu || !c->memory.adapter) return STATUS_INVALID_DEVICE_STATE;
    c->memory.adapter->DmaOperations->FreeCommonBuffer(c->memory.adapter,SIZE,
        c->memory.logical,c->memory.cpu,FALSE);
    RtlZeroMemory(c,sizeof(*c)); return STATUS_SUCCESS;
}
NTSTATUS a04_windows_linear_context_start(struct a04_windows_linear_context *c,
                                         struct a04_gpu_firmware_state *s)
{
    struct a04_gpu_fw_arena arena={0}; struct rogue_fwif_kccb_cmd kick={0};
    uint64_t dma,src[6],dst[6]; uint8_t *p; size_t first,i; NTSTATUS status;
    if (!c || !s || c->firmware || c->memory.adapter || c->memory.cpu || !s->processor_released ||
        !s->firmware_started || s->queue_faulted || !s->memory.adapter || !s->memory.heap ||
        !s->memory.adapter->DmaOperations || !s->memory.adapter->DmaOperations->AllocateCommonBuffer ||
        !s->memory.adapter->DmaOperations->FreeCommonBuffer ||
        s->runtime.next_free>A04_PVR_HEAP_SIZE-PAGE ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    c->firmware=s;
    c->memory.adapter=s->memory.adapter;
    c->memory.cpu=c->memory.adapter->DmaOperations->AllocateCommonBuffer(
        c->memory.adapter,SIZE,&c->memory.logical,FALSE);
    if (!c->memory.cpu) { c->memory.adapter=0; c->firmware=0; return STATUS_INSUFFICIENT_RESOURCES; }
    dma=(uint64_t)c->memory.logical.QuadPart; p=c->memory.cpu;
    if (((uintptr_t)p&(PAGE-1u)) || (dma&(PAGE-1u)) || dma>UINT64_C(0x100000000)-SIZE)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    if (dma<(uint64_t)s->memory.heap_logical.QuadPart+A04_PVR_HEAP_SIZE &&
        (uint64_t)s->memory.heap_logical.QuadPart<dma+SIZE) return STATUS_DEVICE_CONFIGURATION_ERROR;
    if (s->memory.table && dma<(uint64_t)s->memory.table_logical.QuadPart+0x4000u &&
        (uint64_t)s->memory.table_logical.QuadPart<dma+SIZE) return STATUS_DEVICE_CONFIGURATION_ERROR;
    RtlZeroMemory(p,SIZE);
    if (a04_gpu_mmu_map_brn65273((uint32_t *)(void *)p,(uint64_t *)(void *)(p+PAGE),
        (uint64_t *)(void *)(p+2u*PAGE),(uint64_t *)(void *)(p+3u*PAGE),
        (uint64_t *)(void *)(p+4u*PAGE),dma+PAGE,dma+2u*PAGE,dma+3u*PAGE,
        dma+4u*PAGE,dma+5u*PAGE)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    for (i=0;i<a04_gpu_linear_copy_region_count;++i) {
        const struct a04_gpu_copy_region *r=&a04_gpu_linear_copy_regions[i];
        size_t pd=6u+2u*r->group,pt=pd+1u,offset=14u+r->page_offset;
        if (r->group>=A04_GPU_COPY_GROUPS || offset>PAGES || r->page_count>PAGES-offset ||
            a04_gpu_mmu_map_4k((uint32_t *)(void *)p,(uint64_t *)(void *)(p+pd*PAGE),
                (uint64_t *)(void *)(p+pt*PAGE),dma+pd*PAGE,dma+pt*PAGE,r->va,
                dma+offset*PAGE,r->page_count,0)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    if (a04_gpu_linear_copy_fixture(p+14u*PAGE,A04_GPU_LINEAR_COPY_PAGES*PAGE,&c->command))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    first=(s->runtime.next_free+PAGE-1u)&~(size_t)(PAGE-1u);
    if (a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,
            (uint64_t)s->memory.heap_logical.QuadPart,first) ||
        a04_gpu_fw_memory_context(&arena,dma,&c->vm) ||
        a04_gpu_fw_prepare_transfer_queue(&arena,&c->vm,1,1,4,&c->command,&c->queue,&kick))
        return STATUS_INSUFFICIENT_RESOURCES;
    s->runtime.next_free=(uint32_t)arena.next;
#define SYNC(o) do { status=a04_gpu_dma_sync((o).cpu,(ULONG)(o).size,FALSE); if (!NT_SUCCESS(status)) return status; } while (0)
    status=a04_gpu_dma_sync(p,SIZE,FALSE); if (!NT_SUCCESS(status)) return status;
    SYNC(c->vm); SYNC(c->queue.control); SYNC(c->queue.ring); SYNC(c->queue.state);
    SYNC(c->queue.fence); SYNC(c->queue.context);
#undef SYNC
    status=a04_gpu_firmware_invalidate_mmu(s); if (!NT_SUCCESS(status)) return status;
    for (i=0;i<6u;++i) {
        src[i]=dma+(14u+A04_GPU_LINEAR_SOURCE_PAGE+i)*PAGE;
        dst[i]=dma+(14u+A04_GPU_LINEAR_DEST_PAGE+i)*PAGE;
    }
    status=a04_windows_transfer_mapping_init(&c->mapping,s,&c->queue,
        (uint64_t *)(void *)(p+7u*PAGE),src,dst,1u);
    if (!NT_SUCCESS(status)) return status;
    status=a04_gpu_firmware_submit_kick(s,&kick);
    if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
    if (c->mapping.mapping.ops.wait_idle(c->mapping.mapping.context)) return c->mapping.last_status;
    return STATUS_SUCCESS;
}
