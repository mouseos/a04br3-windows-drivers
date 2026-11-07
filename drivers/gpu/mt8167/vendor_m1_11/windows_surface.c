/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_surface.h"
#include "A04BR3RogueDeviceWords.h"
#include "../gpu_mmu.h"
#include "../windows_hw.h"
#define TAG 0x53524741u
static int allocation_overlap(const struct a04_vendor_uncached *a,const struct a04_vendor_uncached *b)
{
    uint64_t x=(uint64_t)a->logical.QuadPart,y=(uint64_t)b->logical.QuadPart;
    uintptr_t u=(uintptr_t)a->cpu,v=(uintptr_t)b->cpu;
    return (x<=y ? y-x<a->bytes : x-y<b->bytes) ||
           (u<=v ? v-u<a->bytes : u-v<b->bytes);
}
NTSTATUS a04_vendor_surface_release(struct a04_vendor_surface *s)
{
    unsigned i;
    if(!s || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(s->hardware_exposed)return STATUS_DEVICE_BUSY;
    for(i=0;i<A04_SURFACE_MAX_LEAVES;i++)a04_vendor_uncached_free(&s->pt[i]);
    a04_vendor_uncached_free(&s->pd);a04_vendor_uncached_free(&s->pc);
    a04_vendor_uncached_free(&s->pixels);RtlZeroMemory(s,sizeof(*s));
    return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_surface_prepare(struct a04_vendor_surface *s,PDMA_ADAPTER adapter,
                                   ULONG width,ULONG height,ULONG pitch,uint64_t va)
{
    struct a04_gpu_mmu_leaf leaves[A04_SURFACE_MAX_LEAVES];
    uint64_t bytes64,pages_count;uint64_t *pages=NULL;void *shadow=NULL;
    uint32_t *pc;uint64_t *pd;unsigned i;ULONG n;NTSTATUS status=STATUS_INVALID_PARAMETER;
    if(!s || !adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL || s->prepared || s->hardware_exposed ||
       s->pixels.cpu || s->pc.cpu || s->pd.cpu || s->pixels.adapter || s->pc.adapter || s->pd.adapter || !width || !height || (uint64_t)width*4u>pitch ||
       (va&4095u) || va>=(UINT64_C(1)<<40))return status;
    for(i=0;i<A04_SURFACE_MAX_LEAVES;i++)if(s->pt[i].cpu || s->pt[i].adapter)return status;
    bytes64=(uint64_t)pitch*height;
    if(!bytes64 || bytes64>8u*1024u*1024u || (bytes64&4095u))return status;
    pages_count=bytes64/4096u;
    n=(ULONG)(((va>>12)&511u)+pages_count+511u)/512u;
    if(!n || n>A04_SURFACE_MAX_LEAVES)return status;
    status=a04_vendor_uncached_allocate(&s->pixels,adapter,(ULONG)bytes64);
    if(!NT_SUCCESS(status))goto fail;
    status=a04_vendor_uncached_allocate(&s->pc,adapter,4096);if(!NT_SUCCESS(status))goto fail;
    if(allocation_overlap(&s->pixels,&s->pc)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto quarantine;}
    status=a04_vendor_uncached_allocate(&s->pd,adapter,4096);if(!NT_SUCCESS(status))goto fail;
    if(allocation_overlap(&s->pixels,&s->pd) || allocation_overlap(&s->pc,&s->pd)) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto quarantine;
    }
    for(i=0;i<n;i++) {
        unsigned j;
        status=a04_vendor_uncached_allocate(&s->pt[i],adapter,4096);if(!NT_SUCCESS(status))goto fail;
        if(allocation_overlap(&s->pt[i],&s->pixels) || allocation_overlap(&s->pt[i],&s->pc) ||
           allocation_overlap(&s->pt[i],&s->pd)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto quarantine;}
        for(j=0;j<i;j++)if(allocation_overlap(&s->pt[i],&s->pt[j])) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto quarantine;}
    }
    if(((uint64_t)s->pixels.logical.QuadPart&4095u) || ((uint64_t)s->pc.logical.QuadPart&4095u) ||
       ((uint64_t)s->pd.logical.QuadPart&4095u)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    shadow=ExAllocatePoolWithTag(NonPagedPoolNx,(2u+n)*4096u,TAG);
    pages=ExAllocatePoolWithTag(NonPagedPoolNx,(size_t)pages_count*sizeof(*pages),TAG);
    if(!shadow || !pages){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    RtlZeroMemory(shadow,(2u+n)*4096u);pc=shadow;pd=(uint64_t *)((UCHAR *)shadow+4096u);
    for(i=0;i<n;i++){leaves[i].cpu=(uint64_t *)((UCHAR *)shadow+(2u+i)*4096u);leaves[i].dma=(uint64_t)s->pt[i].logical.QuadPart;}
    for(i=0;i<pages_count;i++)pages[i]=(uint64_t)s->pixels.logical.QuadPart+(uint64_t)i*4096u;
    if(a04_gpu_mmu_map_pages_multi_4k(pc,pd,(uint64_t)s->pd.logical.QuadPart,leaves,n,va,pages,(size_t)pages_count,A04_GPU_MMU_SLC_BYPASS) ||
       a04_rgx_device_write_words(s->pc.cpu,pc,4096) || a04_rgx_device_write_words(s->pd.cpu,pd,4096)) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;
    }
    for(i=0;i<n;i++)if(a04_rgx_device_write_words(s->pt[i].cpu,leaves[i].cpu,4096)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    a04_gpu_dma_barrier();s->width=width;s->height=height;s->pitch=pitch;s->bytes=(ULONG)bytes64;
    s->gpu_va=va;s->leaves=n;s->prepared=TRUE;status=STATUS_SUCCESS;
    goto fail;
quarantine:
    /* A broken allocator may have returned aliases. Freeing either could
     * invalidate a different live allocation. Retain all of them. */
    s->hardware_exposed=TRUE;
fail:
    if(pages)ExFreePoolWithTag(pages,TAG);
    if(shadow)ExFreePoolWithTag(shadow,TAG);
    if(!NT_SUCCESS(status))(void)a04_vendor_surface_release(s);
    return status;
}
NTSTATUS a04_vendor_surface_mark_exposed(struct a04_vendor_surface *s)
{
    if(!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->prepared || s->hardware_exposed)return STATUS_INVALID_DEVICE_STATE;
    s->hardware_exposed=TRUE;return STATUS_SUCCESS;
}
