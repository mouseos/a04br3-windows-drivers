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
static int read_output_pt(struct a04_vendor_native_cold *n,uint64_t *pt) {
 for(ULONG leaf=0;leaf<A04_NATIVE_OUTPUT_PT_COUNT;leaf++)
  if(!n->output[2+leaf].cpu||a04_rgx_device_read_words(pt+leaf*512u,n->output[2+leaf].cpu,4096u))return 1;
 return 0;
}
static int write_output_pt(struct a04_vendor_native_cold *n,const uint64_t *pt) {
 for(ULONG leaf=0;leaf<A04_NATIVE_OUTPUT_PT_COUNT;leaf++)
  if(a04_rgx_device_write_words(n->output[2+leaf].cpu,pt+leaf*512u,4096u))return 1;
 return 0;
}
static int clash(const struct a04_vendor_uncached *a,const uint64_t *pages,ULONG count,void *cpu,ULONG bytes)
{
    ULONG i;if(!a->cpu)return 0;
    if(a04_rgx_fw_alias(a->cpu,a->bytes,cpu,bytes))return 1;
    for(i=0;i<count;i++)if(a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,pages[i],4096))return 1;
    return 0;
}
static NTSTATUS isolated(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *target)
{
    struct a04_vendor_render_cold *r=n->render;ULONG i,j,count;
    const uint64_t *pages=a04_native_mdl_pages(target,&count);
    ULONG bytes=a04_native_mdl_extent(target);void *cpu=a04_native_mdl_cpu(target);
    if(!pages || !cpu)return STATUS_INVALID_PARAMETER;
    if(a04_rgx_fw_alias(cpu,bytes,n,sizeof(*n)) || a04_rgx_fw_alias(cpu,bytes,r,sizeof(*r)))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<16;i++)if(clash(&r->buffer[i],pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<n->data_blocks;i++)if(clash(a04_vendor_native_block(n,0,i),pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<n->pixel_blocks;i++)if(clash(a04_vendor_native_block(n,1,i),pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    if(clash(&n->pt,pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<2;i++)if(clash(&n->code[i],pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=1;i<A04_NATIVE_OUTPUT_TABLE_COUNT;i++)if(clash(&n->output[i],pages,count,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<5;i++)for(j=0;j<count;j++)
        if(a04_rgx_fw_overlap(r->firmware->buffers[i].dma,r->firmware->buffers[i].bytes,pages[j],4096) ||
           a04_rgx_fw_alias(r->firmware->buffers[i].cpu,r->firmware->buffers[i].bytes,cpu,bytes))return STATUS_DEVICE_CONFIGURATION_ERROR;
    return STATUS_SUCCESS;
}
static int binding_clash(struct a04_native_mdl_binding *a,uint64_t ao,
 struct a04_native_mdl_binding *b,uint64_t bo)
{
    ULONG na,nb,i,j;const uint64_t *pa=a04_native_mdl_pages(a,&na),*pb=a04_native_mdl_pages(b,&nb);
    if(!pa || !pb || a04_rgx_fw_overlap(ao,na*4096u,bo,nb*4096u) ||
       a04_rgx_fw_alias(a04_native_mdl_cpu_identity(a),na*4096u,a04_native_mdl_cpu_identity(b),nb*4096u))return 1;
    for(i=0;i<na;i++)for(j=0;j<nb;j++)if(pa[i]==pb[j])return 1;
    return 0;
}
NTSTATUS a04_vendor_native_target_aperture(const struct a04_vendor_native_cold *n,
    struct a04_native_target_aperture *out)
{
    struct a04_native_target_aperture made={A04_NATIVE_OUTPUT_VA,A04_NATIVE_APERTURE_BYTES,A04_NATIVE_APERTURE_BYTES,UINT64_C(0xffffffff)};
    if(!n || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL || !n->render ||
       !n->render->firmware || n->render->native_owner!=n || !n->prepared ||
       !n->output_mapped || n->quarantined || n->render->poisoned ||
       !n->output[1].cpu || !n->output[2].cpu || !n->output[3].cpu)
        return STATUS_INVALID_DEVICE_STATE;
    *out=made;return STATUS_SUCCESS;
}
static NTSTATUS attach_target(struct a04_vendor_native_cold *n,
    struct a04_native_mdl_binding *target,uint64_t aperture_offset,uint32_t packed_argb,BOOLEAN preserve_active)
{
    struct a04_vendor_render_cold *r;const uint64_t *pages;uint64_t *pt;
    _Alignas(8) uint64_t seen[32];ULONG i,j,count,bytes,first;void *cpu;NTSTATUS status;
    struct a04_m111_clear_surface surface;
    if(!n || !target || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=n->render) || r->native_owner!=n ||
       !r->firmware || r->firmware->hardware_owned || n->quarantined || r->poisoned ||
       n->registered_target || !n->prepared || !n->backing_staged || !n->output_mapped ||
       !n->candidate || !geo_render_layout_valid(n->width,n->height,n->pitch) || !n->pixel_blocks ||
       a04_native_mdl_adapter(target)!=r->firmware->adapter)return STATUS_INVALID_DEVICE_STATE;
    pages=a04_native_mdl_pages(target,&count);bytes=a04_native_mdl_extent(target);cpu=a04_native_mdl_cpu(target);
    if(!pages || !cpu || bytes!=((n->pitch*n->height+4095u)&~4095u) || count!=bytes/4096u)return STATUS_INVALID_PARAMETER;
    if((aperture_offset&4095u) || aperture_offset>A04_NATIVE_APERTURE_BYTES ||
       bytes>A04_NATIVE_APERTURE_BYTES-aperture_offset ||
       a04_m111_build_clear_surface(&surface,A04_NATIVE_OUTPUT_VA+aperture_offset,n->width,n->height,n->pitch/4u))
        return STATUS_INVALID_PARAMETER;
    first=(ULONG)(aperture_offset/4096u);
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
    /* This is the packed-clear owner bind, not a way to relabel the private
     * triangle diagnostic as a D3D clear operation. */
    return STATUS_NOT_SUPPORTED;
#endif
    status=isolated(n,target);if(!NT_SUCCESS(status))return status;
    if(n->registered_vertex && binding_clash(target,aperture_offset,n->registered_vertex,n->vertex_offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(ULONG linear=0;linear<n->linear_count;linear++)
        if(binding_clash(target,aperture_offset,n->linear[linear].binding,n->linear[linear].offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    pt=ExAllocatePoolWithTag(NonPagedPoolNx,A04_NATIVE_OUTPUT_PT_COUNT*4096u,TAG);if(!pt)return STATUS_INSUFFICIENT_RESOURCES;
    status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(read_output_pt(n,pt))goto done;
    for(i=0;i<A04_NATIVE_APERTURE_PAGES;i++){
        uint64_t expected=i<1000?(a04_vendor_native_storage_dma(n,1,i*4096u)|A04_GPU_MMU_SLC_BYPASS|1u):0;
        for(ULONG linear=0;linear<n->linear_count;linear++){
            ULONG extent;const uint64_t *linear_pages=a04_native_mdl_pages(n->linear[linear].binding,&extent);
            ULONG start=(ULONG)(n->linear[linear].offset/4096u);
            if(i>=start && i-start<extent)expected=linear_pages[i-start]|A04_GPU_MMU_SLC_BYPASS|1u;
        }
        if(pt[i]!=expected)goto done;
    }
    status=a04_native_mdl_hold_cold(target);if(!NT_SUCCESS(status))goto done;
    /* Record ownership before any potentially partial PT write. */
    n->registered_target=target;n->target_offset=aperture_offset;
    for(i=0;i<A04_NATIVE_APERTURE_PAGES;i++)pt[i]=i>=first && i-first<count?(pages[i-first]|A04_GPU_MMU_SLC_BYPASS|1u):0;
    if(n->registered_vertex){
        ULONG vertex_count;const uint64_t *vertex_pages=a04_native_mdl_pages(n->registered_vertex,&vertex_count);
        for(i=0;i<vertex_count;i++)pt[n->vertex_offset/4096u+i]=vertex_pages[i]|A04_GPU_MMU_SLC_BYPASS|1u;
    }
    for(ULONG linear=0;linear<n->linear_count;linear++){
        ULONG extent;const uint64_t *linear_pages=a04_native_mdl_pages(n->linear[linear].binding,&extent);
        ULONG start=(ULONG)(n->linear[linear].offset/4096u);
        for(i=0;i<extent;i++)pt[start+i]=linear_pages[i]|A04_GPU_MMU_SLC_BYPASS|1u;
    }
    if(preserve_active)RtlFillMemory((UCHAR *)cpu+n->pitch*n->height,bytes-n->pitch*n->height,0xa5);
    else RtlFillMemory(cpu,bytes,0xa5);
    status=a04_native_mdl_sync_for_gpu(target);if(!NT_SUCCESS(status))goto poison;
    if(write_output_pt(n,pt)){status=STATUS_IO_DEVICE_ERROR;goto poison;}
    a04_gpu_dma_barrier();
    for(i=0;i<A04_NATIVE_APERTURE_PAGES;i+=32){
        if(a04_rgx_device_read_words(seen,(UCHAR *)n->output[2+i/512].cpu+(i%512)*8,sizeof(seen))){status=STATUS_IO_DEVICE_ERROR;goto poison;}
        for(j=0;j<32;j++)if(seen[j]!=pt[i+j]){status=STATUS_IO_DEVICE_ERROR;goto poison;}
    }
    /* Firmware was never started; both PTs now reference only the real MDL.
     * Drop the old private pixel common buffers; retain PD/PT/code/HWRT. */
    a04_vendor_native_storage_free(n,1);
    /* The old job encoded a different output VA/colour. Rebuild and verify
     * every dependent record, EOT/PBE program and ring while FW is still cold.
     * No old private destination survives into an admitted owner job. */
    a04_vendor_native_free_job_cold(n);
    status=a04_vendor_native_prepare_job_cold_colour(n,packed_argb);
    if(!NT_SUCCESS(status))goto poison;
    n->clear_surface=surface;status=STATUS_SUCCESS;goto done;
poison:
    n->quarantined=TRUE;r->poisoned=TRUE;
done:
    ExFreePoolWithTag(pt,TAG);return status;
}
NTSTATUS a04_vendor_native_attach_mdl_target_at_cold(struct a04_vendor_native_cold *n,
    struct a04_native_mdl_binding *target,uint64_t offset,uint32_t colour)
{ return attach_target(n,target,offset,colour,FALSE); }
NTSTATUS a04_vendor_native_attach_mdl_target_preserve_cold(struct a04_vendor_native_cold *n,
    struct a04_native_mdl_binding *target,uint64_t offset,uint32_t colour)
{ return attach_target(n,target,offset,colour,TRUE); }
NTSTATUS a04_vendor_native_recolour_bound_cold(struct a04_vendor_native_cold *n,uint32_t colour)
{
    NTSTATUS status;
    if(!n || !n->registered_target || !n->candidate || !n->render || !n->render->firmware ||
       n->render->firmware->hardware_owned || n->quarantined || n->render->poisoned ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    a04_vendor_native_free_job_cold(n);
    status=a04_vendor_native_prepare_job_cold_colour(n,colour);
    if(!NT_SUCCESS(status)){n->quarantined=TRUE;n->render->poisoned=TRUE;}
    return status;
}
NTSTATUS a04_vendor_native_attach_mdl_target_cold(struct a04_vendor_native_cold *n,
    struct a04_native_mdl_binding *target)
{ return a04_vendor_native_attach_mdl_target_at_cold(n,target,0,0xff332211u); }
NTSTATUS a04_vendor_native_target_before_fw_start(struct a04_vendor_native_cold *n)
{
    if(!n || !n->render || !n->render->firmware || n->render->firmware->hardware_owned ||
       n->quarantined || !n->candidate || ((n->registered_vertex || n->linear_count) && !n->registered_target) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    NTSTATUS status;
    for(ULONG linear=0;linear<n->linear_count;linear++){
        status=a04_native_mdl_sync_for_gpu(n->linear[linear].binding);if(!NT_SUCCESS(status))return status;
        status=a04_native_mdl_mark_exposed(n->linear[linear].binding);if(!NT_SUCCESS(status))return status;
    }
    if(n->registered_vertex){status=a04_native_mdl_sync_for_gpu(n->registered_vertex);if(!NT_SUCCESS(status))return status;
        status=a04_native_mdl_mark_exposed(n->registered_vertex);if(!NT_SUCCESS(status))return status;}
    return n->registered_target?a04_native_mdl_mark_exposed(n->registered_target):STATUS_SUCCESS;
}

NTSTATUS a04_vendor_native_attach_vertex_cold(struct a04_vendor_native_cold *n,
 struct a04_native_mdl_binding *v,uint64_t offset)
{
    ULONG count,i,bytes;uint64_t *pt;const uint64_t *pages;NTSTATUS status;
    if(!n || !v || !n->render || !n->render->firmware || n->render->firmware->hardware_owned ||
       n->quarantined || n->render->poisoned || n->registered_vertex || !n->output_mapped ||
       a04_native_mdl_adapter(v)!=n->render->firmware->adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    pages=a04_native_mdl_pages(v,&count);bytes=a04_native_mdl_extent(v);
    if(!pages || !count || count>16 || bytes!=count*4096u || (offset&4095u) ||
       offset>A04_NATIVE_APERTURE_BYTES || bytes>A04_NATIVE_APERTURE_BYTES-offset)return STATUS_INVALID_PARAMETER;
    status=isolated(n,v);if(!NT_SUCCESS(status))return status;
    if(n->registered_target && binding_clash(n->registered_target,n->target_offset,v,offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(ULONG linear=0;linear<n->linear_count;linear++)
        if(binding_clash(v,offset,n->linear[linear].binding,n->linear[linear].offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
    /* Vertex-first is retained cold; no root can be published until the real
     * target attach replaces private output with the complete union PT. */
    if(!n->registered_target){
        status=a04_native_mdl_hold_cold(v);if(!NT_SUCCESS(status))return status;
        n->registered_vertex=v;n->vertex_offset=offset;return STATUS_SUCCESS;
    }
    pt=ExAllocatePoolWithTag(NonPagedPoolNx,A04_NATIVE_OUTPUT_PT_COUNT*4096u,TAG);if(!pt)return STATUS_INSUFFICIENT_RESOURCES;
    status=STATUS_IO_DEVICE_ERROR;
    if(read_output_pt(n,pt))goto done;
    for(i=0;i<count;i++)if(pt[offset/4096u+i]){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
    status=a04_native_mdl_hold_cold(v);if(!NT_SUCCESS(status))goto done;
    n->registered_vertex=v;n->vertex_offset=offset;
    for(i=0;i<count;i++)pt[offset/4096u+i]=pages[i]|A04_GPU_MMU_SLC_BYPASS|1u;
    status=a04_native_mdl_sync_for_gpu(v);
    if(!NT_SUCCESS(status) || write_output_pt(n,pt))goto poison;
    a04_gpu_dma_barrier();
    for(i=0;i<count;i++){
        uint64_t actual;ULONG at=(ULONG)(offset/4096u)+i;
        if(a04_rgx_device_read_words(&actual,(UCHAR *)n->output[2+at/512].cpu+(at%512)*8,8) || actual!=pt[at])goto poison;
    }
    status=STATUS_SUCCESS;goto done;
poison:n->quarantined=TRUE;n->render->poisoned=TRUE;status=STATUS_IO_DEVICE_ERROR;
done:ExFreePoolWithTag(pt,TAG);return status;
}

/* Real linear mapping of retained VidMm pages, never a target/shader token. */
NTSTATUS a04_vendor_native_attach_linear_cold(struct a04_vendor_native_cold *n,
 struct a04_native_mdl_binding *binding,uint64_t offset)
{
 ULONG count,bytes,i,at;uint64_t *pt;const uint64_t *pages;NTSTATUS status;
 if(!n||!binding||!n->render||!n->render->firmware||n->render->firmware->hardware_owned||
    n->quarantined||n->render->poisoned||!n->output_mapped||n->linear_count>=8||
    a04_native_mdl_adapter(binding)!=n->render->firmware->adapter||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 pages=a04_native_mdl_pages(binding,&count);bytes=a04_native_mdl_extent(binding);
 if(!pages||!count||count>1024||bytes!=count*4096u||(offset&4095u)||offset>A04_NATIVE_APERTURE_BYTES||bytes>A04_NATIVE_APERTURE_BYTES-offset)return STATUS_INVALID_PARAMETER;
 status=isolated(n,binding);if(!NT_SUCCESS(status))return status;
 if(n->registered_target&&binding_clash(binding,offset,n->registered_target,n->target_offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
 if(n->registered_vertex&&binding_clash(binding,offset,n->registered_vertex,n->vertex_offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
 for(i=0;i<n->linear_count;i++)if(binding_clash(binding,offset,n->linear[i].binding,n->linear[i].offset))return STATUS_DEVICE_CONFIGURATION_ERROR;
 pt=ExAllocatePoolWithTag(NonPagedPoolNx,A04_NATIVE_OUTPUT_PT_COUNT*4096u,TAG);if(!pt)return STATUS_INSUFFICIENT_RESOURCES;
 status=STATUS_IO_DEVICE_ERROR;
 if(read_output_pt(n,pt))goto done_linear;
 for(i=0;i<count;i++){
  at=(ULONG)(offset/4096u)+i;
  uint64_t expected=!n->registered_target&&at<1000?(a04_vendor_native_storage_dma(n,1,at*4096u)|A04_GPU_MMU_SLC_BYPASS|1u):0;
  if(pt[at]!=expected){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done_linear;}
 }
 status=a04_native_mdl_hold_cold(binding);if(!NT_SUCCESS(status))goto done_linear;
 /* Preserve the lease before the first potentially partial PTE update. */
 n->linear[n->linear_count].binding=binding;n->linear[n->linear_count++].offset=offset;
 for(i=0;i<count;i++)pt[offset/4096u+i]=pages[i]|A04_GPU_MMU_SLC_BYPASS|1u;
 status=a04_native_mdl_sync_for_gpu(binding);
 if(!NT_SUCCESS(status)||write_output_pt(n,pt))goto poison_linear;
 a04_gpu_dma_barrier();
 for(i=0;i<A04_NATIVE_APERTURE_PAGES;i++){
  uint64_t actual;
  if(a04_rgx_device_read_words(&actual,(UCHAR *)n->output[2+i/512].cpu+(i%512)*8,8)||actual!=pt[i])goto poison_linear;
 }
 status=STATUS_SUCCESS;goto done_linear;
 poison_linear:n->quarantined=TRUE;n->render->poisoned=TRUE;status=STATUS_IO_DEVICE_ERROR;
 done_linear:ExFreePoolWithTag(pt,TAG);return status;
}

/* Actual finish/SLC/GP receipt precedes this helper. Mapping/physical lease
 * stays resident: UNMAP/normal retirement remains the only release path. */
NTSTATUS a04_vendor_native_mdl_cpu_complete(struct a04_vendor_native_cold *n)
{
    NTSTATUS status;
    if(!n || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    if(n->registered_target){status=a04_native_mdl_sync_for_cpu(n->registered_target);if(!NT_SUCCESS(status))return status;}
    if(n->registered_vertex){status=a04_native_mdl_sync_for_cpu(n->registered_vertex);if(!NT_SUCCESS(status))return status;}
    for(ULONG i=0;i<n->linear_count;i++){status=a04_native_mdl_sync_for_cpu(n->linear[i].binding);if(!NT_SUCCESS(status))return status;}
    return STATUS_SUCCESS;
}

/* Serialized after the previous real fence, before any subsequent GPU job.
 * CPU handoff covered all bindings, so next publication must cover all too. */
NTSTATUS a04_vendor_native_mdl_gpu_prepare(struct a04_vendor_native_cold *n)
{
    NTSTATUS status;
    if(!n || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    if(n->registered_target){status=a04_native_mdl_sync_for_gpu(n->registered_target);if(!NT_SUCCESS(status))return status;}
    if(n->registered_vertex){status=a04_native_mdl_sync_for_gpu(n->registered_vertex);if(!NT_SUCCESS(status))return status;}
    for(ULONG i=0;i<n->linear_count;i++){status=a04_native_mdl_sync_for_gpu(n->linear[i].binding);if(!NT_SUCCESS(status))return status;}
    return STATUS_SUCCESS;
}
