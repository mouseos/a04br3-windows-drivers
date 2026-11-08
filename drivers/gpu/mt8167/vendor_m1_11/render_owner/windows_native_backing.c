/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Cold-only private native output and known program/suffix staging.
 * No complete render packet, CCB publication, or MMIO execution. */
#include "windows_native_cold_private.h"
#include "windows_copy_scratch.h"
#include "native_terminate_stream.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../../gpu_mmu.h"
#include "../../windows_hw.h"
#define BACKING_TAG 0x624e4750u
_Static_assert(A04_NATIVE_OUTPUT_BYTES==800u*1280u*4u && !(A04_NATIVE_OUTPUT_BYTES&4095u),"private surface span");
_Static_assert((A04_NATIVE_OUTPUT_VA>>30)!=(A04_NATIVE_DATA_VA>>30) &&
    (A04_NATIVE_OUTPUT_VA>>30)!=(A04_NATIVE_USC_VA>>30) &&
    (A04_NATIVE_OUTPUT_VA>>30)!=(A04_NATIVE_PDS_VA>>30) &&
    (A04_NATIVE_OUTPUT_VA>>30)!=(UINT64_C(0x8000000000)>>30),"private catalogue entry");
static int clash(const struct a04_vendor_uncached *a,const struct a04_vendor_uncached *b)
{
    return a!=b && b->cpu && (a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,
        (uint64_t)b->logical.QuadPart,b->bytes) || a04_rgx_fw_alias(a->cpu,a->bytes,b->cpu,b->bytes));
}
/* Aliased DMA cannot be safely freed even on a cold preparation failure.
 * Quarantine the whole child so parent/FW/adapter teardown stays blocked. */
NTSTATUS a04_vendor_native_validate_allocation_cold(struct a04_vendor_native_cold *n,
    const struct a04_vendor_uncached *a)
{
    unsigned i;struct a04_vendor_render_cold *r=n->render;
    struct a04_vendor_m1_11_memory *f=r->firmware;
    struct a04_vendor_copy_scratch *c=r->copy_owner;
    struct a04_vendor_surface *s=r->screen_surface;
    if(!a->cpu || !a->bytes || a->adapter!=f->adapter || a->logical.QuadPart<=0 ||
       (a->logical.QuadPart&4095u) || ((uintptr_t)a->cpu&7u) ||
       (uint64_t)a->logical.QuadPart>UINT64_C(0x100000000)-a->bytes)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    if(a04_rgx_fw_alias(a->cpu,a->bytes,n,sizeof(*n)) ||
       a04_rgx_fw_alias(a->cpu,a->bytes,r,sizeof(*r)) ||
       a04_rgx_fw_alias(a->cpu,a->bytes,f,sizeof(*f)) ||
       (f->runtime && a04_rgx_fw_alias(a->cpu,a->bytes,f->runtime,sizeof(*f->runtime))) ||
       (c && a04_rgx_fw_alias(a->cpu,a->bytes,c,sizeof(*c))) ||
       (s && a04_rgx_fw_alias(a->cpu,a->bytes,s,sizeof(*s))))goto quarantine;
    if(clash(a,&n->pt))goto quarantine;
    for(i=0;i<n->data_blocks;i++)if(clash(a,a04_vendor_native_block(n,0,i)))goto quarantine;
    for(i=0;i<n->pixel_blocks;i++)if(clash(a,a04_vendor_native_block(n,1,i)))goto quarantine;
    for(i=0;i<2;i++)if(clash(a,&n->code[i]))goto quarantine;
    for(i=0;i<4;i++)if(clash(a,&n->output[i]))goto quarantine;
    for(i=0;i<16;i++)if(clash(a,&r->buffer[i]))goto quarantine;
    for(i=0;i<5;i++)if(f->buffers[i].cpu &&
        (a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,f->buffers[i].dma,f->buffers[i].bytes) ||
         a04_rgx_fw_alias(a->cpu,a->bytes,f->buffers[i].cpu,f->buffers[i].bytes)))goto quarantine;
    if(c && (clash(a,&c->source) || clash(a,&c->destination) || clash(a,&c->shared)))goto quarantine;
    if(s) {
        if(clash(a,&s->pixels) || clash(a,&s->pc) || clash(a,&s->pd))goto quarantine;
        for(i=0;i<A04_SURFACE_MAX_LEAVES;i++)if(clash(a,&s->pt[i]))goto quarantine;
    }
    return STATUS_SUCCESS;
quarantine:
    n->quarantined=TRUE;return STATUS_DEVICE_CONFIGURATION_ERROR;
}
static int write_verified(volatile void *dst,const void *src,unsigned bytes)
{
    _Alignas(8) uint32_t observed[64];const a04_rgx_word *expected=src;unsigned i;
    if(bytes>sizeof(observed) || (bytes&3u) || a04_rgx_device_write_words(dst,src,bytes))return 1;
    a04_gpu_dma_barrier();
    if(a04_rgx_device_read_words(observed,dst,bytes))return 1;
    for(i=0;i<bytes/4u;i++)if(observed[i]!=expected[i])return 1;
    return 0;
}
NTSTATUS a04_vendor_native_unmap_output_cold(struct a04_vendor_native_cold *n)
{
    uint32_t entry=0,zero=0,expected;struct a04_vendor_render_cold *r;
    if(!n || KeGetCurrentIrql()!=PASSIVE_LEVEL || n->quarantined || !(r=n->render) ||
       r->native_owner!=n || !r->firmware || r->firmware->hardware_owned || !n->output_mapped)
        return STATUS_INVALID_DEVICE_STATE;
    expected=(uint32_t)((uint64_t)n->output[1].logical.QuadPart>>8)|1u;
    if(a04_rgx_device_read_words(&entry,(UCHAR *)r->buffer[1].cpu+(A04_NATIVE_OUTPUT_VA>>30)*4u,4u) ||
       (entry && entry!=expected) ||
       write_verified((UCHAR *)r->buffer[1].cpu+(A04_NATIVE_OUTPUT_VA>>30)*4u,&zero,4u))
        return STATUS_IO_DEVICE_ERROR;
    n->output_mapped=FALSE;return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_native_prepare_backing_cold(struct a04_vendor_native_cold *n,
    PDMA_ADAPTER adapter,ULONG maps)
{
    struct a04_vendor_render_cold *r;UCHAR *shadow;
    uint32_t *pc,entry;uint64_t *pd,*pt[2],address;
    _Alignas(8) struct a04_m111_clear_surface surface;
    _Alignas(8) struct a04_m111_clear_codespace code;
    _Alignas(8) struct a04_m111_clear_code_bindings bindings;
    _Alignas(8) struct a04_m111_terminate_stream suffix;
    unsigned i;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(!n || !adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL || n->quarantined ||
       !(r=n->render) || r->native_owner!=n || !r->firmware || r->firmware->adapter!=adapter ||
       r->firmware->hardware_owned || r->firmware->render_owner!=r || !n->prepared || !n->mapped || !n->bank_prepared ||
       n->code_mapped!=3u || n->output_mapped || n->backing_staged || maps<A04_NATIVE_BLOCK_BYTES/4096u ||
       n->width!=800u || n->height!=(A04_GE8300_VENDOR_NATIVE_CLEAR_JOB?600u:1280u) || n->suffix_offset+4096u!=n->used)
        return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<4;i++)if(n->output[i].cpu || n->output[i].adapter)return STATUS_INVALID_DEVICE_STATE;
    a04_vendor_native_diagnose(n,A04_NATIVE_PREP_OUTPUT,0xffffffffu,5u*4096u);
    shadow=ExAllocatePoolWithTag(NonPagedPoolNx,5u*4096u,BACKING_TAG);
    if(!shadow)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(shadow,5u*4096u);pc=(uint32_t *)(void *)shadow;
    pd=(uint64_t *)(void *)(shadow+4096u);pt[0]=(uint64_t *)(void *)(shadow+8192u);
    pt[1]=(uint64_t *)(void *)(shadow+12288u);
    if(a04_rgx_device_read_words(pc,r->buffer[1].cpu,4096u) || pc[A04_NATIVE_OUTPUT_VA>>30])goto done;
    status=a04_vendor_native_storage_allocate(n,1,adapter,A04_NATIVE_OUTPUT_BYTES);
    if(!NT_SUCCESS(status))goto done;
    for(i=1;i<4;i++) {
        a04_vendor_native_diagnose(n,A04_NATIVE_PREP_OUTPUT,64u+i,4096u);
        status=a04_vendor_uncached_allocate(&n->output[i],adapter,4096u);
        if(!NT_SUCCESS(status))goto done;
        status=a04_vendor_native_validate_allocation_cold(n,&n->output[i]);
        if(!NT_SUCCESS(status))goto done;
    }
    status=STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<1000u;i++){
        unsigned leaf=i/512u;address=a04_vendor_native_storage_dma(n,1,i*4096u);
        if(!address || a04_gpu_mmu_map_4k(pc,pd,pt[leaf],(uint64_t)n->output[1].logical.QuadPart,
            (uint64_t)n->output[2u+leaf].logical.QuadPart,A04_NATIVE_OUTPUT_VA+i*4096u,
            address,1u,A04_GPU_MMU_SLC_BYPASS))goto done;
    }
    /* A black clear must change the owned output, rather than matching an
     * already-zero allocation. Table shadows remain zero-initialized. */
    for(i=0;i<1024u;i++)((uint32_t *)(void *)(shadow+16384u))[i]=0xa5a5a5a5u;
    /* All allocations are validated before any Device-memory write. */
    for(i=0;i<1000u;i++)if(a04_vendor_native_storage_write(n,1,i*4096u,shadow+16384u,4096u))goto done;
    if(a04_rgx_device_write_words(n->output[1].cpu,pd,4096u) ||
       a04_rgx_device_write_words(n->output[2].cpu,pt[0],4096u) ||
       a04_rgx_device_write_words(n->output[3].cpu,pt[1],4096u))goto done;
    a04_gpu_dma_barrier();entry=pc[A04_NATIVE_OUTPUT_VA>>30];
    n->output_mapped=TRUE; /* Retain ownership through any partial PC write. */
    if(write_verified((UCHAR *)r->buffer[1].cpu+(A04_NATIVE_OUTPUT_VA>>30)*4u,&entry,4u))goto done;
    if(a04_m111_build_clear_surface(&surface,A04_NATIVE_OUTPUT_VA,800u,n->height,800u) ||
       a04_m111_build_clear_codespace(&code,A04_NATIVE_USC_VA,A04_NATIVE_PDS_VA,0u,surface.emit[0],surface.emit[1]) ||
       a04_m111_build_clear_code_bindings(&bindings,A04_NATIVE_PDS_VA) ||
       a04_m111_build_clear_terminate_stream(&suffix,A04_NATIVE_DATA_VA+n->suffix_offset,800u,n->height))goto done;
    if(write_verified(n->code[0].cpu,code.usc,sizeof(code.usc)) ||
       write_verified(n->code[1].cpu,code.pds,sizeof(code.pds)) ||
       write_verified(a04_vendor_native_storage_pointer(n,0,n->suffix_offset,sizeof(suffix.data)),suffix.data,sizeof(suffix.data)))goto done;
    n->clear_surface=surface;n->bindings=bindings;n->backing_staged=TRUE;status=STATUS_SUCCESS;
done:
    ExFreePoolWithTag(shadow,BACKING_TAG);return status;
}
