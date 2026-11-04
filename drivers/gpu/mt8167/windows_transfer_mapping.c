/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_transfer_mapping.h"
static int wait_idle(void *p)
{
    struct a04_windows_transfer_mapping *c=p; LARGE_INTEGER delay; unsigned i;
    struct rogue_fwif_cccb_ctl *ctl=c->queue->control.cpu;
    delay.QuadPart=-100000;
    for (i=0;i<200u;++i) {
        c->last_status=a04_gpu_dma_sync(c->queue->control.cpu,(ULONG)c->queue->control.size,TRUE);
        if (!NT_SUCCESS(c->last_status)) break;
        c->last_status=a04_gpu_dma_sync(c->queue->fence.cpu,(ULONG)c->queue->fence.size,TRUE);
        if (!NT_SUCCESS(c->last_status)) break;
        if (*(volatile uint32_t *)&ctl->read_offset==c->queue->write_offset &&
            *(volatile uint32_t *)c->queue->fence.cpu==c->completed_fence) return 0;
        c->last_status=KeDelayExecutionThread(KernelMode,FALSE,&delay);
        if (!NT_SUCCESS(c->last_status)) break;
    }
    if (i==200u) c->last_status=STATUS_IO_TIMEOUT;
    c->firmware->queue_faulted=TRUE; return -1;
}
static int sync_table(void *p,void *table,size_t size)
{
    struct a04_windows_transfer_mapping *c=p;
    c->last_status=a04_gpu_dma_sync(table,(ULONG)size,FALSE);
    if (!NT_SUCCESS(c->last_status)) { c->firmware->queue_faulted=TRUE; return -1; }
    return 0;
}
static int invalidate(void *p)
{
    struct a04_windows_transfer_mapping *c=p;
    c->last_status=a04_gpu_firmware_invalidate_mmu(c->firmware);
    if (!NT_SUCCESS(c->last_status)) { c->firmware->queue_faulted=TRUE; return -1; }
    return 0;
}
NTSTATUS a04_windows_transfer_mapping_init(struct a04_windows_transfer_mapping *c,
    struct a04_gpu_firmware_state *s,struct a04_gpu_fw_queue *q,uint64_t *leaf,
    const uint64_t *src,const uint64_t *dst,uint32_t fence)
{
    size_t i;
    if (!c || !s || !q || !leaf || !src || !dst || !s->processor_released ||
        !s->firmware_started || s->queue_faulted || !q->control.cpu || !q->fence.cpu ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(c,sizeof(*c)); c->firmware=s; c->queue=q; c->completed_fence=fence;
    c->mapping.leaf=leaf; c->mapping.context=c; c->mapping.ops.wait_idle=wait_idle;
    c->mapping.ops.sync_table=sync_table; c->mapping.ops.invalidate=invalidate;
    for (i=0;i<A04_TRANSFER_PAGES;++i) {
        c->mapping.scratch_source[i]=src[i]; c->mapping.scratch_dest[i]=dst[i];
    }
    c->last_status=STATUS_SUCCESS; return STATUS_SUCCESS;
}
