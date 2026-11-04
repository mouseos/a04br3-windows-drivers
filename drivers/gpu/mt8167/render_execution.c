/* SPDX-License-Identifier: MIT */
#include "render_execution.h"
#define PAGE UINT64_C(4096)
#define GPU_LIMIT (UINT64_C(1)<<40)
#define DMA_LIMIT (UINT64_C(1)<<32)
int a04_render_validate_owned_ranges(const struct a04_render_owned_range *r,size_t count)
{
    size_t i,j;
    if (!r || !count || count>4096) return -1;
    for (i=0;i<count;++i) {
        const struct a04_render_owned_range *a=&r[i];
        if (!a->size || !a->gpu_va || !a->dma || (a->size&(PAGE-1)) ||
            (a->gpu_va&(PAGE-1)) || (a->dma&(PAGE-1)) ||
            a->gpu_va>=GPU_LIMIT || a->size>GPU_LIMIT-a->gpu_va ||
            a->dma>=DMA_LIMIT || a->size>DMA_LIMIT-a->dma ||
            !(a->access&A04_RENDER_RANGE_READ) || (a->access&~7u) ||
            ((a->access&A04_RENDER_RANGE_EXEC) && (a->access&A04_RENDER_RANGE_WRITE))) return -1;
        for (j=0;j<i;++j) {
            const struct a04_render_owned_range *b=&r[j];
            if ((a->gpu_va < b->gpu_va+b->size && b->gpu_va < a->gpu_va+a->size) ||
                (a->dma < b->dma+b->size && b->dma < a->dma+a->size)) return -1;
        }
    }
    return 0;
}
int a04_render_execute_owned(struct a04_render_execution *s,
    const struct a04_render_owned_range *ranges,size_t count,
    const struct a04_render_execution_ops *o)
{
    if (!s || s->state!=A04_RENDER_IDLE || !o || !o->sync_for_device || !o->publish_mmu ||
        !o->submit || !o->wait_geom || !o->wait_frag || !o->sync_for_cpu ||
        !o->firmware_cleanup || !o->retire_mmu || a04_render_validate_owned_ranges(ranges,count)) return -1;
    if (o->sync_for_device(o->context)) return -1;
    /* From the first publication attempt onward failure is ambiguous. */
    s->state=A04_RENDER_POISONED;
    if (o->publish_mmu(o->context)) return -1;
    s->state=A04_RENDER_PUBLISHED;
    if (o->submit(o->context)) { s->state=A04_RENDER_POISONED; return -1; }
    s->state=A04_RENDER_SUBMITTED;
    if (o->wait_geom(o->context) || o->wait_frag(o->context) || o->sync_for_cpu(o->context)) {
        s->state=A04_RENDER_POISONED; return -1;
    }
    s->state=A04_RENDER_COMPLETED;
    if (o->firmware_cleanup(o->context) || o->retire_mmu(o->context)) {
        s->state=A04_RENDER_POISONED; return -1;
    }
    s->state=A04_RENDER_RETIRED;
    return 0;
}
