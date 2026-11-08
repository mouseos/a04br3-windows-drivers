/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Actual same-MDL target for the validated800x600 profile. No private output
 * is used after the cold PT replacement. No WDDM capability is advertised. */
#include "windows_native_cold_private.h"
#if !A04_GE8300_NATIVE_OWNER_MDL_TARGET
#error Compile this module only for the explicit native MDL target variant
#endif
#include "native_owner_mdl.h"
#include "A04BR3RogueDeviceWords.h"
#include "../../gpu_mmu.h"
#include "../../windows_hw.h"
#define TAG 0x746e4741u
static int clash(const struct a04_vendor_uncached *a,const uint64_t *pages,ULONG count,const void *cpu,ULONG bytes)
{
    ULONG i;if(!a->cpu)return 0;
    if(a04_rgx_fw_alias(a->cpu,a->bytes,cpu,bytes))return 1;
    for(i=0;i<count;i++)if(a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,pages[i],4096))return 1;
    return 0;
}
static NTSTATUS isolated(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *target)
{
    struct a04_vendor_render_cold *r=n->render;ULONG i,j,count;
    const uint64_t *pages=a04_native_mdl_physical_identity(target,&count);
    ULONG bytes=a04_native_mdl_extent(target);const void *cpu=a04_native_mdl_cpu_identity(target);
    if(!pages || !cpu)return STATUS_INVALID_PARAMETER;
    if(a04_rgx_fw_alias(cpu,bytes,n,sizeof(*n)) || a04_rgx_fw_alias(cpu,bytes,r,sizeof(*r)))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<16;i++)if(clash(&r->buffer[i],pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<n->data_blocks;i++)if(clash(a04_vendor_native_block(n,0,i),pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<n->pixel_blocks;i++)if(clash(a04_vendor_native_block(n,1,i),pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    if(clash(&n->pt,pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<2;i++)if(clash(&n->code[i],pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=1;i<4;i++)if(clash(&n->output[i],pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<5;i++)for(j=0;j<count;j++)
        if(a04_rgx_fw_overlap(r->firmware->buffers[i].dma,r->firmware->buffers[i].bytes,pages[j],4096) ||
           a04_rgx_fw_alias(r->firmware->buffers[i].cpu,r->firmware->buffers[i].bytes,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    return STATUS_SUCCESS;
}
static int binding_clash(struct a04_native_mdl_binding *a,uint64_t ao,
 struct a04_native_mdl_binding *b,uint64_t bo)
{
    ULONG na,nb,i,j;const uint64_t *pa=a04_native_mdl_physical_identity(a,&na),*pb=a04_native_mdl_physical_identity(b,&nb);
    if(!pa || !pb || a04_rgx_fw_overlap(ao,na*4096u,bo,nb*4096u) ||
       a04_rgx_fw_alias(a04_native_mdl_cpu_identity(a),na*4096u,a04_native_mdl_cpu_identity(b),nb*4096u))return 1;
    for(i=0;i<na;i++)for(j=0;j<nb;j++)if(pa[i]==pb[j])return 1;
    return 0;
}
NTSTATUS a04_vendor_native_target_aperture(const struct a04_vendor_native_cold *n,
    struct a04_native_target_aperture *out)
{
    struct a04_native_target_aperture made={A04_NATIVE_OUTPUT_VA,1024u*4096u,1024u*4096u,UINT64_C(0xffffffff)};
    if(!n || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL || !n->render ||
       !n->render->firmware || n->render->native_owner!=n || !n->prepared ||
       !n->output_mapped || n->quarantined || n->render->poisoned ||
       !n->output[1].cpu || !n->output[2].cpu || !n->output[3].cpu)
        return STATUS_INVALID_DEVICE_STATE;
    *out=made;return STATUS_SUCCESS;
}
static NTSTATUS attach_target(struct a04_vendor_native_cold *n,
 struct a04_native_mdl_binding *target,uint64_t offset,uint32_t colour,BOOLEAN preserve)
{
    NTSTATUS status;ULONG bytes;void *cpu;struct a04_m111_clear_surface surface;
    if(!n || !target || !n->render || !n->render->firmware || n->render->firmware->hardware_owned ||
       n->quarantined || n->render->poisoned || n->registered_target || !n->output_mapped ||
       !n->candidate || n->width!=800 || n->height!=600 || !n->pixel_blocks ||
       a04_native_mdl_adapter(target)!=n->render->firmware->adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_DEVICE_STATE;
    bytes=a04_native_mdl_extent(target);cpu=a04_native_mdl_cpu(target);
    if(!cpu || bytes!=((1920000u+4095u)&~4095u) || (offset&4095u) || offset>4194304u || bytes>4194304u-offset ||
       a04_m111_build_clear_surface(&surface,A04_NATIVE_OUTPUT_VA+offset,800,600,800))return STATUS_INVALID_PARAMETER;
    status=isolated(n,target);if(!NT_SUCCESS(status))return status;
    if(n->registered_vertex && binding_clash(target,offset,n->registered_vertex,n->vertex_offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(ULONG i=0;i<n->linear_count;i++)if(binding_clash(target,offset,n->linear[i].binding,n->linear[i].offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    status=a04_native_mdl_hold_cold(target);if(!NT_SUCCESS(status))return status;
    n->registered_target=target;n->target_offset=offset;
    if(preserve)RtlFillMemory((UCHAR *)cpu+1920000u,bytes-1920000u,0xa5);else RtlFillMemory(cpu,bytes,0xa5);
    /* No DMA lease/PT publication during CPU preparation. The old private
     * output is cold; all real MDL PTEs are rebuilt at before_fw_start. */
    a04_vendor_native_storage_free(n,1);a04_vendor_native_free_job_cold(n);
    status=a04_vendor_native_prepare_job_cold_colour(n,colour);
    if(!NT_SUCCESS(status)){n->quarantined=TRUE;n->render->poisoned=TRUE;return status;}
    n->clear_surface=surface;return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_native_attach_mdl_target_at_cold(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *b,uint64_t o,uint32_t c)
{return attach_target(n,b,o,c,FALSE);}
NTSTATUS a04_vendor_native_attach_mdl_target_preserve_cold(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *b,uint64_t o,uint32_t c)
{return attach_target(n,b,o,c,TRUE);}
NTSTATUS a04_vendor_native_attach_mdl_target_cold(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *b)
{return attach_target(n,b,0,0xff332211u,FALSE);}
NTSTATUS a04_vendor_native_recolour_bound_cold(struct a04_vendor_native_cold *n,uint32_t c)
{
    NTSTATUS status;
    if(!n || !n->registered_target || !n->candidate || !n->render || !n->render->firmware ||
       n->render->firmware->hardware_owned || n->quarantined || n->render->poisoned ||
       !a04_native_mdl_cpu(n->registered_target) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    a04_vendor_native_free_job_cold(n);status=a04_vendor_native_prepare_job_cold_colour(n,c);
    if(!NT_SUCCESS(status)){n->quarantined=TRUE;n->render->poisoned=TRUE;}return status;
}
static ULONG binding_set(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding **b,uint64_t *offset)
{
    ULONG count=0;
    if(n->registered_target){b[count]=n->registered_target;offset[count++]=n->target_offset;}
    if(n->registered_vertex){b[count]=n->registered_vertex;offset[count++]=n->vertex_offset;}
    for(ULONG i=0;i<n->linear_count;i++){b[count]=n->linear[i].binding;offset[count++]=n->linear[i].offset;}
    return count;
}
NTSTATUS a04_vendor_native_target_acquire_pt_quiescent(struct a04_vendor_native_cold *n)
{
    struct a04_native_mdl_binding *b[10];uint64_t offsets[10],*pt;ULONG count,acquired=0,i,j;NTSTATUS status=STATUS_SUCCESS;
    if(!n || !n->render || !n->render->firmware ||
       n->quarantined || n->render->poisoned || !n->candidate ||
       ((n->registered_vertex || n->linear_count) && !n->registered_target) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    if(!n->registered_target)return STATUS_SUCCESS; /* cold startup private backing */
    count=binding_set(n,b,offsets);pt=ExAllocatePoolWithTag(NonPagedPoolNx,8192,TAG);if(!pt)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(pt,8192);
    for(i=0;i<count;i++){
        ULONG pages;const uint64_t *dma;
        status=a04_native_mdl_sync_for_gpu(b[i]);if(!NT_SUCCESS(status))goto unwind;
        ++acquired;dma=a04_native_mdl_pages(b[i],&pages);
        if(!dma || offsets[i]/4096u>1024u || pages>1024u-offsets[i]/4096u){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto unwind;}
        for(j=0;j<pages;j++){
            ULONG index=(ULONG)(offsets[i]/4096u)+j;
            if(pt[index]){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto unwind;}
            pt[index]=dma[j]|A04_GPU_MMU_SLC_BYPASS|1u;
        }
    }
    /* Before any PTE write mark every backing retained against ambiguous
     * partial publication. No CPU alias is now accessible through our API. */
    for(i=0;i<count;i++){status=a04_native_mdl_mark_exposed(b[i]);if(!NT_SUCCESS(status))goto poison;}
    if(a04_rgx_device_write_words(n->output[2].cpu,pt,4096) || a04_rgx_device_write_words(n->output[3].cpu,pt+512,4096)){status=STATUS_IO_DEVICE_ERROR;goto poison;}
    a04_gpu_dma_barrier();
    for(i=0;i<1024;i++){uint64_t value;
        if(a04_rgx_device_read_words(&value,(UCHAR *)n->output[2+i/512].cpu+(i%512)*8,8) || value!=pt[i]){status=STATUS_IO_DEVICE_ERROR;goto poison;}}
    ExFreePoolWithTag(pt,TAG);return STATUS_SUCCESS;
unwind:
    while(acquired){NTSTATUS clean=a04_native_mdl_sync_for_cpu(b[--acquired]);if(!NT_SUCCESS(clean))status=clean;}
    ExFreePoolWithTag(pt,TAG);return status;
poison:
    n->quarantined=TRUE;n->render->poisoned=TRUE;ExFreePoolWithTag(pt,TAG);return status;
}
NTSTATUS a04_vendor_native_target_before_fw_start(struct a04_vendor_native_cold *n)
{
    if(!n || !n->render || !n->render->firmware || n->render->firmware->hardware_owned)return STATUS_INVALID_DEVICE_STATE;
    return a04_vendor_native_target_acquire_pt_quiescent(n);
}
NTSTATUS a04_vendor_native_attach_vertex_cold(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *v,uint64_t o)
{
    NTSTATUS status;ULONG bytes;
    if(!n || !v || !n->render || !n->render->firmware || n->render->firmware->hardware_owned ||
       n->quarantined || n->render->poisoned || n->registered_vertex || !n->output_mapped ||
       a04_native_mdl_adapter(v)!=n->render->firmware->adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    bytes=a04_native_mdl_extent(v);
    if(!a04_native_mdl_cpu(v) || !bytes || bytes>65536u || (o&4095u) || o>4194304u || bytes>4194304u-o)return STATUS_INVALID_PARAMETER;
    status=isolated(n,v);if(!NT_SUCCESS(status))return status;
    if(n->registered_target && binding_clash(n->registered_target,n->target_offset,v,o))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(ULONG i=0;i<n->linear_count;i++)if(binding_clash(v,o,n->linear[i].binding,n->linear[i].offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    status=a04_native_mdl_hold_cold(v);if(!NT_SUCCESS(status))return status;
    n->registered_vertex=v;n->vertex_offset=o;return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_native_attach_linear_cold(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *b,uint64_t o)
{
    NTSTATUS status;ULONG bytes;
    if(!n || !b || !n->render || !n->render->firmware || n->render->firmware->hardware_owned ||
       n->quarantined || n->render->poisoned || !n->output_mapped || n->linear_count>=8 ||
       a04_native_mdl_adapter(b)!=n->render->firmware->adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    bytes=a04_native_mdl_extent(b);
    if(!a04_native_mdl_cpu(b) || !bytes || bytes>4194304u || (o&4095u) || o>4194304u || bytes>4194304u-o)return STATUS_INVALID_PARAMETER;
    status=isolated(n,b);if(!NT_SUCCESS(status))return status;
    if(n->registered_target && binding_clash(b,o,n->registered_target,n->target_offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    if(n->registered_vertex && binding_clash(b,o,n->registered_vertex,n->vertex_offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(ULONG i=0;i<n->linear_count;i++)if(binding_clash(b,o,n->linear[i].binding,n->linear[i].offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    status=a04_native_mdl_hold_cold(b);if(!NT_SUCCESS(status))return status;
    n->linear[n->linear_count].binding=b;n->linear[n->linear_count++].offset=o;return STATUS_SUCCESS;
}
/* Caller owns command_busy and has actual TA/3D, cleanup, SLC and GP completion.
 * Remove ALL MDL PTEs before asking the FW to invalidate its translation cache.
 * A timeout retains every DMA resource; zero PTE memory alone is not enough. */
NTSTATUS a04_vendor_native_target_remove_pt(struct a04_vendor_native_cold *n)
{
    uint64_t zero[32]={0},value;ULONG i;
    if(!n || !n->registered_target || !n->render || !n->render->firmware ||
       !n->render->firmware->hardware_owned || n->quarantined || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<8192;i+=sizeof(zero))if(a04_rgx_device_write_words((UCHAR *)n->output[2+i/4096].cpu+i%4096,zero,sizeof(zero)))return STATUS_IO_DEVICE_ERROR;
    a04_gpu_dma_barrier();
    for(i=0;i<1024;i++)if(a04_rgx_device_read_words(&value,(UCHAR *)n->output[2+i/512].cpu+(i%512)*8,8) || value)return STATUS_IO_DEVICE_ERROR;
    return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_native_target_cpu_after_mmu_ack(struct a04_vendor_native_cold *n)
{
    struct a04_native_mdl_binding *b[10];uint64_t o[10];ULONG count,i;NTSTATUS status;
    if(!n || !n->registered_target || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    count=binding_set(n,b,o);
    for(i=0;i<count;i++){
        status=a04_native_mdl_detach_after_mmu_ack(b[i]);if(!NT_SUCCESS(status))return status;
        status=a04_native_mdl_sync_for_cpu(b[i]);if(!NT_SUCCESS(status))return status;
    }
    return STATUS_SUCCESS;
}
