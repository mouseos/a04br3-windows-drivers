/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Reserve the proven native USC/PDS code spans before first firmware start.
 * No program/packet is supplied, no queue is published and no MMIO is kicked. */
#include "windows_native_cold_private.h"
#include "windows_copy_scratch.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../gpu_mmu.h"
#include "../windows_hw.h"
#define CODE_TAG 0x634e4750u
static const uint64_t code_va[2]={UINT64_C(0xba00010000),UINT64_C(0xa800010000)};
static const unsigned directory[2]={8,5},leaf[2]={9,6};
static int conflict(const struct a04_vendor_uncached *a,const struct a04_vendor_uncached *b)
{
    return b->cpu && (a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,
        (uint64_t)b->logical.QuadPart,b->bytes) || a04_rgx_fw_alias(a->cpu,a->bytes,b->cpu,b->bytes));
}
NTSTATUS a04_vendor_native_unmap_code_cold(struct a04_vendor_native_cold *n)
{
    uint64_t zero=0;unsigned i;struct a04_vendor_render_cold *r;
    if(!n || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=n->render) || r->native_owner!=n ||
       !r->firmware || r->firmware->hardware_owned)return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<2;i++)if(n->code_mapped&(1u<<i)) {
        if(a04_rgx_device_write_words((UCHAR *)r->buffer[leaf[i]].cpu+
            (((code_va[i]>>12)&511u)*8u),&zero,8u))return STATUS_IO_DEVICE_ERROR;
        a04_gpu_dma_barrier();n->code_mapped&=~(1u<<i);
    }
    return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_native_reserve_code_cold(struct a04_vendor_native_cold *n,PDMA_ADAPTER adapter)
{
    UCHAR *shadow;uint32_t *pc;uint64_t *pd,*pt;unsigned i,j;NTSTATUS status;
    struct a04_vendor_render_cold *r;struct a04_vendor_m1_11_memory *f;
    struct a04_vendor_copy_scratch *copy;
    if(!n || !adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=n->render) ||
       r->native_owner!=n || !n->prepared || !n->mapped || !(f=r->firmware) ||
       f->hardware_owned || f->adapter!=adapter || n->code_mapped || n->code[0].cpu || n->code[1].cpu)
        return STATUS_INVALID_DEVICE_STATE;
    shadow=ExAllocatePoolWithTag(NonPagedPoolNx,4u*4096u,CODE_TAG);
    if(!shadow)return STATUS_INSUFFICIENT_RESOURCES;
    pc=(uint32_t *)(void *)shadow;pd=(uint64_t *)(void *)(shadow+4096u);
    pt=(uint64_t *)(void *)(shadow+8192u);copy=r->copy_owner;
    for(i=0;i<2;i++) {
        status=a04_vendor_uncached_allocate(&n->code[i],adapter,4096u);
        if(!NT_SUCCESS(status))goto done;
        status=STATUS_DEVICE_CONFIGURATION_ERROR;
        if(n->code[i].logical.QuadPart<=0 || (n->code[i].logical.QuadPart&4095u) ||
           ((uintptr_t)n->code[i].cpu&7u) || conflict(&n->code[i],&n->data) ||
           conflict(&n->code[i],&n->pt))goto done;
        for(j=0;j<i;j++)if(conflict(&n->code[i],&n->code[j]))goto done;
        for(j=0;j<16;j++)if(conflict(&n->code[i],&r->buffer[j]))goto done;
        for(j=0;j<5;j++)if(a04_rgx_fw_overlap((uint64_t)n->code[i].logical.QuadPart,4096u,
            f->buffers[j].dma,f->buffers[j].bytes) ||
            a04_rgx_fw_alias(n->code[i].cpu,4096u,f->buffers[j].cpu,f->buffers[j].bytes))goto done;
        if(copy && (conflict(&n->code[i],&copy->source) || conflict(&n->code[i],&copy->destination) ||
            conflict(&n->code[i],&copy->shared)))goto done;
        RtlZeroMemory(shadow+12288u,4096u);
        if(!r->buffer[directory[i]].cpu || !r->buffer[leaf[i]].cpu ||
           a04_rgx_device_read_words(pc,r->buffer[1].cpu,4096u) ||
           a04_rgx_device_read_words(pd,r->buffer[directory[i]].cpu,4096u) ||
           a04_rgx_device_read_words(pt,r->buffer[leaf[i]].cpu,4096u) ||
           pc[code_va[i]>>30]!=((uint32_t)((uint64_t)r->buffer[directory[i]].logical.QuadPart>>8)|1u) ||
           !pd[(code_va[i]>>21)&511u] || pt[(code_va[i]>>12)&511u] ||
           a04_gpu_mmu_map_4k(pc,pd,pt,(uint64_t)r->buffer[directory[i]].logical.QuadPart,
             (uint64_t)r->buffer[leaf[i]].logical.QuadPart,code_va[i],
             (uint64_t)n->code[i].logical.QuadPart,1u,
             A04_GPU_MMU_SLC_BYPASS|(i==0?A04_GPU_MMU_READ_ONLY:0u)) ||
           a04_rgx_device_write_words(n->code[i].cpu,shadow+12288u,4096u))goto done;
        a04_gpu_dma_barrier();
        /* Claim before the write so even a partial failure retains ownership. */
        n->code_mapped|=1u<<i;
        if(a04_rgx_device_write_words((UCHAR *)r->buffer[leaf[i]].cpu+
            (((code_va[i]>>12)&511u)*8u),pt+((code_va[i]>>12)&511u),8u))goto done;
        a04_gpu_dma_barrier();
    }
    status=STATUS_SUCCESS;
done:
    ExFreePoolWithTag(shadow,CODE_TAG);return status;
}
