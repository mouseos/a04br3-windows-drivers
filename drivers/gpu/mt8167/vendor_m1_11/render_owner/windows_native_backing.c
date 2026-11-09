/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Cold-only private native output and known program/suffix staging.
 * No complete render packet, CCB publication, or MMIO execution. */
#include "windows_native_cold_private.h"
#include "../../start_checkpoint357.h"
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
    for(i=0;i<A04_NATIVE_OUTPUT_TABLE_COUNT;i++)if(clash(a,&n->output[i]))goto quarantine;
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
    uint32_t *pc,entry;uint64_t *pd,*pt[A04_NATIVE_OUTPUT_PT_COUNT],address;
    _Alignas(8) struct a04_m111_clear_surface surface;
    _Alignas(8) struct a04_m111_clear_codespace code;
    _Alignas(8) struct a04_m111_clear_code_bindings bindings;
    _Alignas(8) struct a04_m111_terminate_stream suffix;
    unsigned i;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;struct geo_start357_record obs;int api;
    Geo357Begin(&obs,1);obs.words[0]=maps;obs.words[1]=A04_NATIVE_OUTPUT_BYTES;obs.words[2]=A04_NATIVE_OUTPUT_BYTES/4096u;obs.words[3]=A04_NATIVE_OUTPUT_PT_COUNT;obs.words[4]=A04_NATIVE_OUTPUT_TABLE_COUNT;
    /* words[20]=0: no SG list exists in this CommonBuffer cold path. */
    if(!n || !adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL || n->quarantined ||
       !(r=n->render) || r->native_owner!=n || !r->firmware || r->firmware->adapter!=adapter ||
       r->firmware->hardware_owned || r->firmware->render_owner!=r || !n->prepared || !n->mapped || !n->bank_prepared ||
       n->code_mapped!=3u || n->output_mapped || n->backing_staged || maps<A04_NATIVE_BLOCK_BYTES/4096u ||
       n->width!=A04_NATIVE_FRAME_WIDTH || n->height!=(A04_GE8300_VENDOR_NATIVE_CLEAR_JOB?A04_NATIVE_FRAME_HEIGHT:1280u) || n->suffix_offset+4096u!=n->used)
        {status=Geo357Fail(&obs,101,STATUS_INVALID_DEVICE_STATE);Geo357End(&obs,status);Geo357Store(&obs,0,0);return status;}
    obs.words[5]=n->width;obs.words[6]=n->height;obs.words[7]=n->used;obs.words[8]=n->data_blocks;
    for(i=0;i<A04_NATIVE_OUTPUT_TABLE_COUNT;i++)if(n->output[i].cpu || n->output[i].adapter){obs.words[10]=i;status=Geo357Fail(&obs,102,STATUS_INVALID_DEVICE_STATE);Geo357End(&obs,status);Geo357Store(&obs,0,0);return status;}
    a04_vendor_native_diagnose(n,A04_NATIVE_PREP_OUTPUT,0xffffffffu,(3u+A04_NATIVE_OUTPUT_PT_COUNT)*4096u);
    shadow=ExAllocatePoolWithTag(NonPagedPoolNx,(3u+A04_NATIVE_OUTPUT_PT_COUNT)*4096u,BACKING_TAG);
    if(!shadow){status=Geo357Fail(&obs,103,STATUS_INSUFFICIENT_RESOURCES);Geo357End(&obs,status);Geo357Store(&obs,0,0);return status;}
    RtlZeroMemory(shadow,(3u+A04_NATIVE_OUTPUT_PT_COUNT)*4096u);pc=(uint32_t *)(void *)shadow;
    pd=(uint64_t *)(void *)(shadow+4096u);for(i=0;i<A04_NATIVE_OUTPUT_PT_COUNT;i++)pt[i]=(uint64_t *)(void *)(shadow+(2u+i)*4096u);
    api=a04_rgx_device_read_words(pc,r->buffer[1].cpu,4096u);obs.words[12]=(ULONG)api;
    if(api){Geo357Fail(&obs,110,status);goto done;}
    obs.words[13]=pc[A04_NATIVE_OUTPUT_VA>>30]?1u:0u;
    if(pc[A04_NATIVE_OUTPUT_VA>>30]){Geo357Fail(&obs,111,status);goto done;}
    status=a04_vendor_native_storage_allocate(n,1,adapter,A04_NATIVE_OUTPUT_BYTES);
    if(!NT_SUCCESS(status)){Geo357Fail(&obs,120,status);goto done;}
    for(i=1;i<A04_NATIVE_OUTPUT_TABLE_COUNT;i++) {
        a04_vendor_native_diagnose(n,A04_NATIVE_PREP_OUTPUT,64u+i,4096u);
        status=a04_vendor_uncached_allocate(&n->output[i],adapter,4096u);
        if(!NT_SUCCESS(status)){obs.words[10]=i;Geo357Fail(&obs,130,status);goto done;}
        status=a04_vendor_native_validate_allocation_cold(n,&n->output[i]);
        if(!NT_SUCCESS(status)){const struct a04_vendor_uncached *b=&n->output[i];obs.words[10]=i;
            obs.words[14]=(b->cpu?1u:0u)|(b->bytes?2u:0u)|(b->adapter==adapter?4u:0u)|(b->logical.QuadPart>0?8u:0u)|(!(b->logical.QuadPart&4095u)?16u:0u)|(!((ULONG_PTR)b->cpu&7u)?32u:0u)|((ULONGLONG)b->logical.QuadPart<=UINT64_C(0x100000000)-b->bytes?64u:0u)|(n->quarantined?128u:0u);
            Geo357Fail(&obs,131,status);goto done;}
    }
    status=STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<1000u;i++){
        unsigned leaf=i/512u;obs.words[10]=i;obs.words[11]=leaf;obs.words[15]=i%512u;
        address=a04_vendor_native_storage_dma(n,1,i*4096u);
        obs.words[16]=(address?1u:0u)|(!(address&4095u)?2u:0u)|(address<UINT64_C(0x10000000000)?4u:0u);
        obs.words[17]=(pc[A04_NATIVE_OUTPUT_VA>>30]?1u:0u)|(pd[((A04_NATIVE_OUTPUT_VA+i*4096u)>>21)&511u]?2u:0u)|(pt[leaf][i%512u]?4u:0u);
        if(!address){Geo357Fail(&obs,140,status);goto done;}
        api=a04_gpu_mmu_map_4k(pc,pd,pt[leaf],(uint64_t)n->output[1].logical.QuadPart,
            (uint64_t)n->output[2u+leaf].logical.QuadPart,A04_NATIVE_OUTPUT_VA+i*4096u,
            address,1u,A04_GPU_MMU_SLC_BYPASS);obs.words[12]=(ULONG)api;
        if(api){Geo357Fail(&obs,141,status);goto done;}
    }
    /* A black clear must change the owned output, rather than matching an
     * already-zero allocation. Table shadows remain zero-initialized. */
    for(i=0;i<1024u;i++)((uint32_t *)(void *)(shadow+(2u+A04_NATIVE_OUTPUT_PT_COUNT)*4096u))[i]=0xa5a5a5a5u;
    /* All allocations are validated before any Device-memory write. */
    for(i=0;i<1000u;i++)if((api=a04_vendor_native_storage_write(n,1,i*4096u,shadow+(2u+A04_NATIVE_OUTPUT_PT_COUNT)*4096u,4096u))){obs.words[10]=i;obs.words[12]=(ULONG)api;Geo357Fail(&obs,150,status);goto done;}
    /* Publish every owned aperture leaf, including currently empty leaves.
     * Later resident primary/target/VB bindings must never write an unlinked PT. */
    for(i=0;i<A04_NATIVE_OUTPUT_PT_COUNT;i++) {
        pd[((A04_NATIVE_OUTPUT_VA>>21)&511u)+i]=(uint64_t)n->output[2u+i].logical.QuadPart|1u;
        if((api=a04_rgx_device_write_words(n->output[2u+i].cpu,pt[i],4096u))){obs.words[10]=i;obs.words[12]=(ULONG)api;Geo357Fail(&obs,160,status);goto done;}
    }
    if((api=a04_rgx_device_write_words(n->output[1].cpu,pd,4096u))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,161,status);goto done;}
    a04_gpu_dma_barrier();entry=pc[A04_NATIVE_OUTPUT_VA>>30];
    n->output_mapped=TRUE; /* Retain ownership through any partial PC write. */
    if((api=write_verified((UCHAR *)r->buffer[1].cpu+(A04_NATIVE_OUTPUT_VA>>30)*4u,&entry,4u))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,162,status);goto done;}
    if((api=a04_m111_build_clear_surface(&surface,A04_NATIVE_OUTPUT_VA,800u,n->height,800u))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,170,status);goto done;}
    if((api=a04_m111_build_clear_codespace(&code,A04_NATIVE_USC_VA,A04_NATIVE_PDS_VA,0u,surface.emit[0],surface.emit[1]))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,171,status);goto done;}
    if((api=a04_m111_build_clear_code_bindings(&bindings,A04_NATIVE_PDS_VA))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,172,status);goto done;}
    if((api=a04_m111_build_clear_terminate_stream(&suffix,A04_NATIVE_DATA_VA+n->suffix_offset,800u,n->height))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,173,status);goto done;}
    if((api=write_verified(n->code[0].cpu,code.usc,sizeof(code.usc)))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,180,status);goto done;}
    if((api=write_verified(n->code[1].cpu,code.pds,sizeof(code.pds)))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,181,status);goto done;}
    if((api=write_verified(a04_vendor_native_storage_pointer(n,0,n->suffix_offset,sizeof(suffix.data)),suffix.data,sizeof(suffix.data)))){obs.words[12]=(ULONG)api;Geo357Fail(&obs,182,status);goto done;}
    n->clear_surface=surface;n->bindings=bindings;n->backing_staged=TRUE;status=STATUS_SUCCESS;
done:
    obs.words[9]=n->pixel_blocks;obs.words[18]=n->quarantined;obs.words[19]=(n->output_mapped?1u:0u)|(n->backing_staged?2u:0u);obs.valid_flags=1;if(NT_SUCCESS(status))obs.last_step=190;
    ExFreePoolWithTag(shadow,BACKING_TAG);Geo357End(&obs,status);Geo357Store(&obs,0,0);return status;
}
