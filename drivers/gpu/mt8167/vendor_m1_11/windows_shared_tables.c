/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_shared_tables.h"
#include "A04BR3RogueDeviceWords.h"
#include "A04BR3RogueFirmware.h"
#include "../windows_hw.h"
#define TAG 0x54444741u
static int conflicts(const struct a04_vendor_uncached *a,uint64_t dma,const void *cpu,size_t bytes)
{
    return a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,dma,bytes) ||
        (cpu && a04_rgx_fw_alias(a->cpu,a->bytes,cpu,bytes));
}
NTSTATUS a04_shared_tables_release(struct a04_shared_tables *t)
{
    unsigned i;
    if(!t || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(t->quarantined)return STATUS_DEVICE_BUSY;
    for(i=0;i<4;i++)a04_vendor_uncached_free(&t->table[i]);
    RtlZeroMemory(t,sizeof(*t));return STATUS_SUCCESS;
}
NTSTATUS a04_shared_tables_stage(struct a04_shared_tables *t,struct a04_shared_scanout *s,
    struct a04_vendor_render_cold *r,struct a04_vendor_copy_scratch *copy)
{
    void *shadow=NULL;unsigned i,j;NTSTATUS status=STATUS_INVALID_PARAMETER;
    struct a04_vendor_uncached *owned[3];
    if(!t || !s || !r || !copy || KeGetCurrentIrql()!=PASSIVE_LEVEL || t->ready || t->quarantined ||
       !s->mdl || !s->gpu_sg || !a04_shared_has_display_backing(s) || !s->gpu_pages || s->exposed ||
       !r->firmware || !r->firmware->adapter || s->gpu_adapter!=r->firmware->adapter ||
       copy->render!=r || r->copy_owner!=copy)return status;
    for(i=0;i<4;i++)if(t->table[i].cpu || t->table[i].adapter)return status;
    owned[0]=&copy->source;owned[1]=&copy->destination;owned[2]=&copy->shared;
    /* A faulty DMA mapping may alias firmware, queues or copy resources. Do
     * not release that mapping until independent quiescence is established. */
    for(i=0;i<A04_SHARED_SCANOUT_PAGES;i++) {
        for(j=0;j<5;j++)if(r->firmware->buffers[j].bytes &&
            a04_rgx_fw_overlap(s->gpu_pages[i],4096,r->firmware->buffers[j].dma,r->firmware->buffers[j].bytes))goto alias;
        for(j=0;j<16;j++)if(r->buffer[j].cpu &&
            a04_rgx_fw_overlap(s->gpu_pages[i],4096,(uint64_t)r->buffer[j].logical.QuadPart,r->buffer[j].bytes))goto alias;
        for(j=0;j<3;j++)if(owned[j]->cpu &&
            a04_rgx_fw_overlap(s->gpu_pages[i],4096,(uint64_t)owned[j]->logical.QuadPart,owned[j]->bytes))goto alias;
    }
    for(i=0;i<4;i++) {
        status=a04_vendor_uncached_allocate(&t->table[i],s->gpu_adapter,4096);
        if(!NT_SUCCESS(status))goto fail;
        for(j=0;j<i;j++)if(conflicts(&t->table[i],(uint64_t)t->table[j].logical.QuadPart,t->table[j].cpu,4096))goto alias;
        for(j=0;j<5;j++)if(conflicts(&t->table[i],r->firmware->buffers[j].dma,r->firmware->buffers[j].cpu,r->firmware->buffers[j].bytes))goto alias;
        for(j=0;j<16;j++)if(r->buffer[j].cpu && conflicts(&t->table[i],(uint64_t)r->buffer[j].logical.QuadPart,r->buffer[j].cpu,r->buffer[j].bytes))goto alias;
        for(j=0;j<3;j++)if(owned[j]->cpu && conflicts(&t->table[i],(uint64_t)owned[j]->logical.QuadPart,owned[j]->cpu,owned[j]->bytes))goto alias;
        for(j=0;j<A04_SHARED_SCANOUT_PAGES;j++)if((uint64_t)t->table[i].logical.QuadPart==s->gpu_pages[j])goto alias;
    }
    shadow=ExAllocatePoolWithTag(NonPagedPoolNx,16384,TAG);
    if(!shadow){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    RtlZeroMemory(shadow,16384);
    status=a04_shared_scanout_build_tables(s,(uint32_t *)shadow,(uint64_t)t->table[0].logical.QuadPart,
        (uint64_t *)((UCHAR *)shadow+4096),(uint64_t)t->table[1].logical.QuadPart,
        (uint64_t *)((UCHAR *)shadow+8192),(uint64_t)t->table[2].logical.QuadPart,
        (uint64_t *)((UCHAR *)shadow+12288),(uint64_t)t->table[3].logical.QuadPart);
    if(!NT_SUCCESS(status))goto fail;
    for(i=0;i<4;i++)if(a04_rgx_device_write_words(t->table[i].cpu,(UCHAR *)shadow+i*4096,4096)) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;
    }
    a04_gpu_dma_barrier();t->ready=TRUE;status=STATUS_SUCCESS;goto out;
alias:
    t->quarantined=TRUE;status=STATUS_DEVICE_CONFIGURATION_ERROR;
fail:
    if(!t->quarantined)(void)a04_shared_tables_release(t);
out:
    if(shadow)ExFreePoolWithTag(shadow,TAG);
    return status;
}
