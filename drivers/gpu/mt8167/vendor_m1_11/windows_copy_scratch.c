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
#define EOT_PDS_OFFSET 64u
#define FRAGMENT_PDS_OFFSET 192u
#define SECONDARY_DATA_OFFSET 256u
#define SECONDARY_CODE_OFFSET 272u
#define FRAGMENT_USC_OFFSET 128u
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
/* Check only our reserved PDS spans. The legacy clear at offset zero is
 * untouched. These images are partial resources, never a runnable task. */
static int pds_region(struct a04_vendor_copy_scratch *o,int write,int clear)
{
    struct a04_vendor_render_cold *r=o->render;uint32_t sample[16],zero[16]={0};unsigned k;
    struct part { unsigned offset,bytes;const uint32_t *data; } part[4]={
        {EOT_PDS_OFFSET,64,o->eot_pds},{FRAGMENT_PDS_OFFSET,64,o->kick.pds_fragment},
        {SECONDARY_DATA_OFFSET,16,o->state.pds_data},{SECONDARY_CODE_OFFSET,16,o->state.pds_code}};
    if(!r || !r->buffer[7].cpu || r->buffer[7].bytes!=65536u)return 1;
    for(k=0;k<4;k++) {
        void *at=(UCHAR*)r->buffer[7].cpu+part[k].offset;
        if(write) { if(a04_rgx_device_write_words(at,clear?zero:part[k].data,part[k].bytes))return 1; }
        else {
            unsigned i;if(a04_rgx_device_read_words(sample,at,part[k].bytes))return 1;
            for(i=0;i<part[k].bytes/4u;i++)if(sample[i]!=(clear?0u:part[k].data[i]))return 1;
        }
    }
    return 0;
}
NTSTATUS a04_vendor_copy_scratch_prepare(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_render_cold *r,ULONG maps)
{
    struct a04_vendor_uncached *b[3];struct a04_vendor_m1_11_memory *f;
    _Alignas(8) uint64_t entries[3]={0}, observed[3]={0};_Alignas(8) struct a04_vendor_copy_state_input in;
    _Alignas(8) struct a04_vendor_clear_image eot;
    struct a04_vendor_copy_kick_input kick={FRAGMENT_PDS_OFFSET,SECONDARY_DATA_OFFSET,SECONDARY_CODE_OFFSET,FRAGMENT_USC_OFFSET};
    uint32_t existing_usc[3];
    unsigned i,j;NTSTATUS status;
    if(!o || !r || KeGetCurrentIrql()!=PASSIVE_LEVEL || o->prepared || o->render || o->exposed || o->usc_ready ||
       o->source.cpu || o->destination.cpu || o->shared.cpu || o->source.adapter || o->destination.adapter || o->shared.adapter ||
       o->source.bytes || o->destination.bytes || o->shared.bytes || r->copy_owner || r->screen_surface ||
       !r->mapped || !r->clear_ready || !(f=r->firmware) || f->hardware_owned || f->render_owner!=r ||
       !f->adapter || !f->staged || !f->uncached || maps<1 || !same_mapping(r) ||
       !r->buffer[7].cpu || r->buffer[7].bytes!=65536u || !r->buffer[10].cpu || r->buffer[10].bytes!=65536u)return STATUS_INVALID_DEVICE_STATE;
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
    if(a04_vendor_copy_kick_build(&o->kick,&kick) || a04_vendor_clear_image_build(&eot,A04_VENDOR_RENDER_SCRATCH_VA,0) ||
       a04_rgx_device_read_words(existing_usc,r->buffer[10].cpu,sizeof(existing_usc))) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    for(i=0;i<3;i++)if(existing_usc[i]!=eot.usc[i]) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    for(i=0;i<16;i++)o->eot_pds[i]=eot.pds[i];
    for(i=0;i<3;i++)o->eot_usc[i]=eot.usc[i];
    for(i=0;i<4;i++)o->destination_csr[i]=eot.payload[32+i];
    /* Fixed native22 profile: source property 0 maps to one output slot
     * (FUN_12e318 table[0]); full 32x32 clip yields the same 2x2 tiles as
     * the restricted clear image. These are partial fields, not a task. */
    o->tile_raw04=eot.payload[1];o->tile_raw18=eot.payload[6];o->tile_raw20=eot.payload[8];
    /* FUN_129f88/13c634: single output, five data words padded to eight,
     * then two code words; offsets are relative to the PDS heap. */
    o->eot_raw28=eot.payload[10];
    o->eot_raw2c=EOT_PDS_OFFSET+eot.payload[11];
    o->eot_raw30=EOT_PDS_OFFSET+eot.payload[12];
    o->eot_pds[2]=(uint32_t)(A04_VENDOR_COPY_DEST_VA>>2);
    o->eot_pds[3]=(uint32_t)(A04_VENDOR_COPY_DEST_VA>>34)|(1u<<19);
    o->render=r;
    if(pds_region(o,0,1)) {o->render=NULL;status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    o->render=NULL;
    for(i=0;i<PAGE_BYTES/4u;i++) {
        ((volatile uint32_t*)o->source.cpu)[i]=a04_vendor_copy_pattern(i);
        ((volatile uint32_t*)o->destination.cpu)[i]=0xa5a5a5a5u;
        ((volatile uint32_t*)o->shared.cpu)[i]=0xa5a5a5a5u;
    }
    if(a04_rgx_device_write_words(o->shared.cpu,o->state.shared,sizeof(o->state.shared))) {status=STATUS_IO_DEVICE_ERROR;goto fail;}
    for(i=0;i<3;i++)entries[i]=(uint64_t)b[i]->logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u;
    /* Claim the parent before publication. On any unexpected write failure,
     * retain everything rather than risk freeing a partially mapped page. */
    o->render=r;r->copy_owner=o;o->prepared=TRUE;a04_gpu_dma_barrier();
    if(pds_region(o,1,0) || pds_region(o,0,0) || ptes(o,entries,1)) {o->exposed=TRUE;return STATUS_IO_DEVICE_ERROR;}
    a04_gpu_dma_barrier();
    if(ptes(o,observed,0) || observed[0]!=entries[0] || observed[1]!=entries[1] || observed[2]!=entries[2]) {
        o->exposed=TRUE;return STATUS_IO_DEVICE_ERROR;
    }
    /* FUN_129f88 starts raw60=3 and changes it only for an alias mapping.
     * Our distinct VA/allocations passed mapping and non-overlap checks. */
    o->raw60=3u;return STATUS_SUCCESS;
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
NTSTATUS a04_vendor_copy_scratch_stage_usc(struct a04_vendor_copy_scratch *o,const void *asset,size_t bytes)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_m1_11_memory *f;
    _Alignas(8) uint32_t image[38],observed[38];unsigned i;
    if(!o || !asset || bytes!=sizeof(image) || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
       !(r=o->render) || !o->prepared || o->exposed || o->usc_ready || r->copy_owner!=o ||
       !(f=r->firmware) || f->render_owner!=r || f->hardware_owned || !r->mapped || !r->clear_ready ||
       !r->buffer[10].cpu || r->buffer[10].bytes!=65536u || !a04_rgx_fw_pointer(asset,bytes) ||
       a04_rgx_fw_alias(asset,bytes,o,sizeof(*o)) || a04_rgx_fw_alias(asset,bytes,r,sizeof(*r)) ||
       a04_rgx_fw_alias(asset,bytes,f,sizeof(*f)) ||
       (f->runtime && a04_rgx_fw_alias(asset,bytes,f->runtime,sizeof(*f->runtime))))return STATUS_INVALID_PARAMETER;
    for(i=0;i<16;i++)if(r->buffer[i].cpu && a04_rgx_fw_alias(asset,bytes,r->buffer[i].cpu,r->buffer[i].bytes))return STATUS_INVALID_PARAMETER;
    for(i=0;i<5;i++)if(!f->allocation[i] || a04_rgx_fw_alias(asset,bytes,f->allocation[i],fw_sizes[i]))return STATUS_INVALID_PARAMETER;
    if(a04_rgx_fw_alias(asset,bytes,o->source.cpu,o->source.bytes) ||
       a04_rgx_fw_alias(asset,bytes,o->destination.cpu,o->destination.bytes) ||
       a04_rgx_fw_alias(asset,bytes,o->shared.cpu,o->shared.bytes))return STATUS_INVALID_PARAMETER;
    RtlCopyMemory(image,asset,sizeof(image));
    if(!a04_vendor_copy_usc_match((const uint8_t *)image,sizeof(image)))return STATUS_DATA_ERROR;
    if(a04_rgx_device_read_words(observed,(UCHAR*)r->buffer[10].cpu+FRAGMENT_USC_OFFSET,sizeof(observed)))return STATUS_IO_DEVICE_ERROR;
    for(i=0;i<38;i++)if(observed[i])return STATUS_DEVICE_CONFIGURATION_ERROR;
    RtlCopyMemory(o->fragment_usc,image,sizeof(image));
    /* Any partial publication retains the owner permanently. */
    if(a04_rgx_device_write_words((UCHAR*)r->buffer[10].cpu+FRAGMENT_USC_OFFSET,image,sizeof(image)))goto retain;
    a04_gpu_dma_barrier();
    if(a04_rgx_device_read_words(observed,(UCHAR*)r->buffer[10].cpu+FRAGMENT_USC_OFFSET,sizeof(observed)))goto retain;
    for(i=0;i<38;i++)if(observed[i]!=image[i])goto retain;
    a04_gpu_dma_barrier();o->usc_ready=TRUE;return STATUS_SUCCESS;
retain:
    o->exposed=TRUE;return STATUS_IO_DEVICE_ERROR;
}
static void snapshot32(struct a04_vendor_copy_task_snapshot *s,unsigned byte_offset,uint32_t value)
{ unsigned word=byte_offset/4u;s->words[word]=value;s->known_words|=UINT64_C(1)<<word; }
static void snapshot64(struct a04_vendor_copy_task_snapshot *s,unsigned byte_offset,uint64_t value)
{ snapshot32(s,byte_offset,(uint32_t)value);snapshot32(s,byte_offset+4u,(uint32_t)(value>>32)); }
static int copy_producer_valid(const struct a04_vendor_copy_scratch *o)
{
    _Alignas(8) struct a04_vendor_copy_state_input in={A04_VENDOR_COPY_SOURCE_VA,A04_VENDOR_COPY_DEST_VA,A04_VENDOR_COPY_SHARED_VA};
    struct a04_vendor_copy_kick_input ki={FRAGMENT_PDS_OFFSET,SECONDARY_DATA_OFFSET,SECONDARY_CODE_OFFSET,FRAGMENT_USC_OFFSET};
    struct a04_vendor_copy_state state;_Alignas(8) struct a04_vendor_copy_kick_image kick;
    _Alignas(8) struct a04_vendor_clear_image eot;unsigned i;
    if(a04_vendor_copy_state_build(&state,&in) || a04_vendor_copy_kick_build(&kick,&ki) ||
       a04_vendor_clear_image_build(&eot,A04_VENDOR_RENDER_SCRATCH_VA,0))return 0;
    eot.pds[2]=(uint32_t)(A04_VENDOR_COPY_DEST_VA>>2);
    eot.pds[3]=(uint32_t)(A04_VENDOR_COPY_DEST_VA>>34)|(1u<<19);
    for(i=0;i<9;i++)if(state.shared[i]!=o->state.shared[i])return 0;
    for(i=0;i<4;i++)if(state.pds_data[i]!=o->state.pds_data[i] || state.pds_code[i]!=o->state.pds_code[i])return 0;
    for(i=0;i<16;i++)if(kick.pds_fragment[i]!=o->kick.pds_fragment[i] || eot.pds[i]!=o->eot_pds[i])return 0;
    for(i=0;i<3;i++)if(eot.usc[i]!=o->eot_usc[i])return 0;
    return kick.raw00==o->kick.raw00 && kick.raw38==o->kick.raw38 && kick.raw40==o->kick.raw40 &&
        kick.raw48==o->kick.raw48 && kick.raw50==o->kick.raw50 && kick.raw58==o->kick.raw58 &&
        o->tile_raw04==eot.payload[1] && o->tile_raw18==eot.payload[6] && o->tile_raw20==eot.payload[8] &&
        o->eot_raw28==eot.payload[10] && o->eot_raw2c==EOT_PDS_OFFSET+eot.payload[11] &&
        o->eot_raw30==EOT_PDS_OFFSET+eot.payload[12];
}
NTSTATUS a04_vendor_copy_scratch_task_snapshot(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_copy_task_snapshot *out)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_m1_11_memory *f;
    _Alignas(8) struct a04_vendor_copy_task_snapshot image={0};
    _Alignas(8) uint64_t entries[3];_Alignas(8) uint32_t usc[38],shared[9],eot[3];unsigned i;
    if(!o || !out || ((uintptr_t)out&7u) || !a04_rgx_fw_pointer(out,sizeof(*out)) ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=o->render) || !o->prepared || o->exposed || !o->usc_ready ||
       r->copy_owner!=o || !(f=r->firmware) || f->render_owner!=r || f->hardware_owned || !r->mapped ||
       !r->clear_ready || !r->buffer[10].cpu || r->buffer[10].bytes!=65536u || !same_mapping(r) ||
       a04_rgx_fw_alias(out,sizeof(*out),o,sizeof(*o)) || a04_rgx_fw_alias(out,sizeof(*out),r,sizeof(*r)) ||
       a04_rgx_fw_alias(out,sizeof(*out),f,sizeof(*f)) ||
       (f->runtime && a04_rgx_fw_alias(out,sizeof(*out),f->runtime,sizeof(*f->runtime))))return STATUS_INVALID_PARAMETER;
    for(i=0;i<16;i++)if(r->buffer[i].cpu && a04_rgx_fw_alias(out,sizeof(*out),r->buffer[i].cpu,r->buffer[i].bytes))return STATUS_INVALID_PARAMETER;
    for(i=0;i<5;i++)if(!f->allocation[i] || a04_rgx_fw_alias(out,sizeof(*out),f->allocation[i],fw_sizes[i]))return STATUS_INVALID_PARAMETER;
    if(a04_rgx_fw_alias(out,sizeof(*out),o->source.cpu,o->source.bytes) ||
       a04_rgx_fw_alias(out,sizeof(*out),o->destination.cpu,o->destination.bytes) ||
       a04_rgx_fw_alias(out,sizeof(*out),o->shared.cpu,o->shared.bytes) || !copy_producer_valid(o) || pds_region(o,0,0) || ptes(o,entries,0) ||
       entries[0]!=((uint64_t)o->source.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) ||
       entries[1]!=((uint64_t)o->destination.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) ||
       entries[2]!=((uint64_t)o->shared.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) ||
       a04_rgx_device_read_words(usc,(UCHAR*)r->buffer[10].cpu+FRAGMENT_USC_OFFSET,sizeof(usc)) ||
       a04_rgx_device_read_words(eot,r->buffer[10].cpu,sizeof(eot)) ||
       a04_rgx_device_read_words(shared,o->shared.cpu,sizeof(shared)))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<38;i++)if(usc[i]!=o->fragment_usc[i])return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<3;i++)if(eot[i]!=o->eot_usc[i])return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<9;i++)if(shared[i]!=o->state.shared[i])return STATUS_DEVICE_CONFIGURATION_ERROR;
    if(!a04_vendor_copy_usc_match((const uint8_t*)usc,sizeof(usc)))return STATUS_DEVICE_CONFIGURATION_ERROR;
    snapshot32(&image,0x00,o->kick.raw00);snapshot32(&image,0x04,o->tile_raw04);
    snapshot32(&image,0x18,o->tile_raw18);snapshot32(&image,0x20,o->tile_raw20);
    snapshot32(&image,0x28,o->eot_raw28);snapshot32(&image,0x2c,o->eot_raw2c);snapshot32(&image,0x30,o->eot_raw30);
    snapshot64(&image,0x38,o->kick.raw38);snapshot64(&image,0x40,o->kick.raw40);
    snapshot64(&image,0x48,o->kick.raw48);snapshot64(&image,0x50,o->kick.raw50);
    snapshot64(&image,0x58,o->kick.raw58);
    /* raw60 remains a mapping-identity candidate, not a proven word. No
     * defaulting of unproven ISP/CSR/tail to valid words either. */
    *out=image;return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_copy_scratch_release(struct a04_vendor_copy_scratch *o)
{
    struct a04_vendor_render_cold *r;_Alignas(8) uint64_t e[3],zero[3]={0};
    _Alignas(8) uint32_t usc[38],empty[38]={0};unsigned i;
    if(!o || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=o->render) || !o->prepared || r->copy_owner!=o ||
       !r->firmware || r->firmware->render_owner!=r)return STATUS_INVALID_DEVICE_STATE;
    if(o->exposed || r->firmware->hardware_owned)return STATUS_DEVICE_BUSY;
    if(!r->mapped || !same_mapping(r) || pds_region(o,0,0) || ptes(o,e,0) ||
       e[0]!=((uint64_t)o->source.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) ||
       e[1]!=((uint64_t)o->destination.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) ||
       e[2]!=((uint64_t)o->shared.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u))return STATUS_INVALID_DEVICE_STATE;
    if(o->usc_ready) {
        if(a04_rgx_device_read_words(usc,(UCHAR*)r->buffer[10].cpu+FRAGMENT_USC_OFFSET,sizeof(usc)))return STATUS_IO_DEVICE_ERROR;
        for(i=0;i<38;i++)if(usc[i]!=o->fragment_usc[i])return STATUS_INVALID_DEVICE_STATE;
        if(a04_rgx_device_write_words((UCHAR*)r->buffer[10].cpu+FRAGMENT_USC_OFFSET,empty,sizeof(empty))) {o->exposed=TRUE;return STATUS_IO_DEVICE_ERROR;}
        a04_gpu_dma_barrier();
        if(a04_rgx_device_read_words(usc,(UCHAR*)r->buffer[10].cpu+FRAGMENT_USC_OFFSET,sizeof(usc))) {o->exposed=TRUE;return STATUS_IO_DEVICE_ERROR;}
        for(i=0;i<38;i++)if(usc[i]) {o->exposed=TRUE;return STATUS_IO_DEVICE_ERROR;}
    }
    if(ptes(o,zero,1))return STATUS_IO_DEVICE_ERROR;
    a04_gpu_dma_barrier();if(ptes(o,e,0) || e[0] || e[1] || e[2] || pds_region(o,1,1) || pds_region(o,0,1))return STATUS_IO_DEVICE_ERROR;
    a04_gpu_dma_barrier();
    a04_vendor_uncached_free(&o->source);a04_vendor_uncached_free(&o->destination);a04_vendor_uncached_free(&o->shared);
    r->copy_owner=NULL;RtlZeroMemory(o,sizeof(*o));return STATUS_SUCCESS;
}
