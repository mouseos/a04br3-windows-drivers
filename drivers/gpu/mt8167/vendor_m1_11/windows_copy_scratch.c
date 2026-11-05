/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Cold scratch allocation and GPU page-table ownership, not a runnable task.
 */
#include "windows_copy_scratch.h"
#include "A04BR3RogueDeviceWords.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "../gpu_mmu.h"
#include "../windows_hw.h"
#define PAGE_BYTES 4096u
#define COPY_PTE_INDEX 16u
static const ULONG fw_sizes[5]={2u*A04_VENDOR_TABLE_BYTES,A04_VENDOR_CODE_BYTES,
    A04_VENDOR_DATA_BYTES,A04_RGX_MAIN_BYTES,A04_RGX_CONFIG_BYTES};
static int clash(const struct a04_vendor_uncached *a,const void *cpu,uint64_t dma,size_t n)
{
    return a04_rgx_fw_alias(a->cpu,a->bytes,cpu,n) ||
           a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,dma,n);
}
static int same_mapping(struct a04_vendor_render_cold *r)
{
    uint32_t pc=0;uint64_t pd=0;
    size_t pci=(size_t)(A04_VENDOR_COPY_SOURCE_VA>>30);
    size_t pdi=(size_t)((A04_VENDOR_COPY_SOURCE_VA>>21)&511u);
    if(!r->buffer[1].cpu || !r->buffer[2].cpu || !r->buffer[3].cpu ||
       r->buffer[1].bytes!=PAGE_BYTES || r->buffer[2].bytes!=PAGE_BYTES || r->buffer[3].bytes!=PAGE_BYTES ||
       a04_rgx_device_read_words(&pc,(UCHAR*)r->buffer[1].cpu+pci*4u,4) ||
       a04_rgx_device_read_words(&pd,(UCHAR*)r->buffer[2].cpu+pdi*8u,8))return 0;
    return pc==((uint32_t)((uint64_t)r->buffer[2].logical.QuadPart>>8)|1u) &&
           pd==((uint64_t)r->buffer[3].logical.QuadPart|1u);
}
static int ptes(struct a04_vendor_copy_scratch *o,uint64_t entries[3],int write)
{
    void *at=(UCHAR*)o->render->buffer[3].cpu+COPY_PTE_INDEX*8u;
    return write ? a04_rgx_device_write_words(at,entries,24) : a04_rgx_device_read_words(entries,at,24);
}
NTSTATUS a04_vendor_copy_scratch_prepare(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_render_cold *r,ULONG maps)
{
    struct a04_vendor_uncached *b[3];struct a04_vendor_m1_11_memory *f;
    _Alignas(8) uint64_t entries[3]={0};_Alignas(8) struct a04_vendor_copy_state_input in;
    unsigned i,j;NTSTATUS status;
    if(!o || !r || KeGetCurrentIrql()!=PASSIVE_LEVEL || o->prepared || o->render || o->exposed ||
       o->source.cpu || o->destination.cpu || o->shared.cpu || o->source.adapter || o->destination.adapter || o->shared.adapter ||
       o->source.bytes || o->destination.bytes || o->shared.bytes || r->copy_owner || r->screen_surface ||
       !r->mapped || !r->clear_ready || !(f=r->firmware) || f->hardware_owned || f->render_owner!=r ||
       !f->adapter || !f->staged || !f->uncached || maps<1 || !same_mapping(r))return STATUS_INVALID_DEVICE_STATE;
    if(!a04_rgx_fw_pointer(o,sizeof(*o)) || !a04_rgx_fw_pointer(r,sizeof(*r)) ||
       a04_rgx_fw_alias(o,sizeof(*o),r,sizeof(*r)) || a04_rgx_fw_alias(o,sizeof(*o),f,sizeof(*f)) ||
       (f->runtime && a04_rgx_fw_alias(o,sizeof(*o),f->runtime,sizeof(*f->runtime))))return STATUS_INVALID_PARAMETER;
    for(i=0;i<16;i++)if(r->buffer[i].cpu && a04_rgx_fw_alias(o,sizeof(*o),r->buffer[i].cpu,r->buffer[i].bytes))return STATUS_INVALID_PARAMETER;
    for(i=0;i<5;i++)if(!f->allocation[i] || a04_rgx_fw_alias(o,sizeof(*o),f->allocation[i],fw_sizes[i]))return STATUS_INVALID_PARAMETER;
    b[0]=&o->source;b[1]=&o->destination;b[2]=&o->shared;
    if(ptes(&(struct a04_vendor_copy_scratch){.render=r},entries,0) || entries[0] || entries[1] || entries[2])
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<3;i++) {
        status=a04_vendor_uncached_allocate(b[i],f->adapter,PAGE_BYTES);
        if(!NT_SUCCESS(status))goto fail;
        if(((uintptr_t)b[i]->cpu&4095u) || ((uint64_t)b[i]->logical.QuadPart&4095u)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
        if(a04_rgx_fw_alias(b[i]->cpu,b[i]->bytes,o,sizeof(*o)) ||
           a04_rgx_fw_alias(b[i]->cpu,b[i]->bytes,r,sizeof(*r)) ||
           a04_rgx_fw_alias(b[i]->cpu,b[i]->bytes,f,sizeof(*f)) ||
           (f->runtime && a04_rgx_fw_alias(b[i]->cpu,b[i]->bytes,f->runtime,sizeof(*f->runtime)))) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto quarantine;}
        for(j=0;j<i;j++)if(clash(b[i],b[j]->cpu,(uint64_t)b[j]->logical.QuadPart,b[j]->bytes)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto quarantine;}
        for(j=0;j<16;j++)if(r->buffer[j].cpu && clash(b[i],r->buffer[j].cpu,(uint64_t)r->buffer[j].logical.QuadPart,r->buffer[j].bytes)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto quarantine;}
        for(j=0;j<5;j++)if(!f->allocation[j] || clash(b[i],f->allocation[j],(uint64_t)f->logical[j].QuadPart,fw_sizes[j])) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto quarantine;}
    }
    in.source_va=A04_VENDOR_COPY_SOURCE_VA;in.destination_va=A04_VENDOR_COPY_DEST_VA;in.shared_va=A04_VENDOR_COPY_SHARED_VA;
    if(a04_vendor_copy_state_build(&o->state,&in)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    for(i=0;i<PAGE_BYTES/4u;i++) {
        ((volatile uint32_t*)o->source.cpu)[i]=0x5a00c3a5u^(i*0x01010101u);
        ((volatile uint32_t*)o->destination.cpu)[i]=0xa5a5a5a5u;
        ((volatile uint32_t*)o->shared.cpu)[i]=0xa5a5a5a5u;
    }
    if(a04_rgx_device_write_words(o->shared.cpu,o->state.shared,sizeof(o->state.shared))) {status=STATUS_IO_DEVICE_ERROR;goto fail;}
    for(i=0;i<3;i++)entries[i]=(uint64_t)b[i]->logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u;
    /* Claim the parent before publication. On any unexpected write failure,
     * retain everything rather than risk freeing a partially mapped page. */
    o->render=r;r->copy_owner=o;o->prepared=TRUE;a04_gpu_dma_barrier();
    if(ptes(o,entries,1))return STATUS_IO_DEVICE_ERROR;
    a04_gpu_dma_barrier();return STATUS_SUCCESS;
quarantine:
    /* An alias means FreeCommonBuffer could release another live object.
     * Retain every allocation and block parent teardown; no PTE is published. */
    o->render=r;o->prepared=TRUE;o->exposed=TRUE;r->copy_owner=o;
    return status;
fail:
    for(i=0;i<3;i++)a04_vendor_uncached_free(b[i]);
    RtlZeroMemory(o,sizeof(*o));return status;
}
NTSTATUS a04_vendor_copy_scratch_mark_exposed(struct a04_vendor_copy_scratch *o)
{
    if(!o || KeGetCurrentIrql()!=PASSIVE_LEVEL || !o->prepared || o->exposed || !o->render ||
       o->render->copy_owner!=o || !o->render->firmware || o->render->firmware->hardware_owned)return STATUS_INVALID_DEVICE_STATE;
    o->exposed=TRUE;return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_copy_scratch_release(struct a04_vendor_copy_scratch *o)
{
    struct a04_vendor_render_cold *r;_Alignas(8) uint64_t e[3],zero[3]={0};
    if(!o || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=o->render) || !o->prepared || r->copy_owner!=o ||
       !r->firmware || r->firmware->render_owner!=r)return STATUS_INVALID_DEVICE_STATE;
    if(o->exposed || r->firmware->hardware_owned)return STATUS_DEVICE_BUSY;
    if(!r->mapped || !same_mapping(r) || ptes(o,e,0) ||
       e[0]!=((uint64_t)o->source.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) ||
       e[1]!=((uint64_t)o->destination.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) ||
       e[2]!=((uint64_t)o->shared.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u))return STATUS_INVALID_DEVICE_STATE;
    if(ptes(o,zero,1))return STATUS_IO_DEVICE_ERROR;
    a04_gpu_dma_barrier();if(ptes(o,e,0) || e[0] || e[1] || e[2])return STATUS_IO_DEVICE_ERROR;
    a04_vendor_uncached_free(&o->source);a04_vendor_uncached_free(&o->destination);a04_vendor_uncached_free(&o->shared);
    r->copy_owner=NULL;RtlZeroMemory(o,sizeof(*o));return STATUS_SUCCESS;
}
