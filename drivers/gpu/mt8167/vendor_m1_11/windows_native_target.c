/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Actual same-MDL target for the validated800x600 profile. No private output
 * is used after the cold PT replacement. No WDDM capability is advertised. */
#include "windows_native_cold_private.h"
#if !A04_GE8300_NATIVE_OWNER_MDL_TARGET
#error Compile this module only for the explicit native MDL target variant
#endif
#include "native_owner_mdl.h"
#include "A04BR3RogueDeviceWords.h"
#include "../gpu_mmu.h"
#include "../windows_hw.h"
#define TAG 0x746e4741u
static int clash(const struct a04_vendor_uncached *a,const uint64_t *pages,ULONG count,void *cpu,ULONG bytes)
{
    ULONG i;if(!a->cpu)return 0;
    if(a04_rgx_fw_alias(a->cpu,a->bytes,cpu,bytes))return 1;
    for(i=0;i<count;i++)if(a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,pages[i],4096))return 1;
    return 0;
}
NTSTATUS a04_vendor_native_attach_mdl_target_cold(struct a04_vendor_native_cold *n,
    struct a04_native_mdl_binding *target)
{
    struct a04_vendor_render_cold *r;const uint64_t *pages;uint64_t *pt;
    _Alignas(8) uint64_t seen[32];ULONG i,j,count,bytes;void *cpu;NTSTATUS status;
    if(!n || !target || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=n->render) || r->native_owner!=n ||
       !r->firmware || r->firmware->hardware_owned || n->quarantined || r->poisoned ||
       n->registered_target || !n->prepared || !n->backing_staged || !n->output_mapped ||
       !n->candidate || n->width!=800 || n->height!=600 || !n->pixel_blocks ||
       a04_native_mdl_adapter(target)!=r->firmware->adapter)return STATUS_INVALID_DEVICE_STATE;
    pages=a04_native_mdl_pages(target,&count);bytes=a04_native_mdl_extent(target);cpu=a04_native_mdl_cpu(target);
    if(!pages || !cpu || bytes!=((800u*600u*4u+4095u)&~4095u) || count!=bytes/4096u)return STATUS_INVALID_PARAMETER;
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
    pt=ExAllocatePoolWithTag(NonPagedPoolNx,8192,TAG);if(!pt)return STATUS_INSUFFICIENT_RESOURCES;
    status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(a04_rgx_device_read_words(pt,n->output[2].cpu,4096) ||
       a04_rgx_device_read_words(pt+512,n->output[3].cpu,4096))goto done;
    for(i=0;i<1024;i++){
        uint64_t expected=i<1000?(a04_vendor_native_storage_dma(n,1,i*4096u)|A04_GPU_MMU_SLC_BYPASS|1u):0;
        if(pt[i]!=expected)goto done;
    }
    status=a04_native_mdl_hold_cold(target);if(!NT_SUCCESS(status))goto done;
    /* Record ownership before any potentially partial PT write. */
    n->registered_target=target;
    for(i=0;i<1024;i++)pt[i]=i<count?(pages[i]|A04_GPU_MMU_SLC_BYPASS|1u):0;
    RtlFillMemory(cpu,bytes,0xa5);
    status=a04_native_mdl_sync_for_gpu(target);if(!NT_SUCCESS(status))goto poison;
    if(a04_rgx_device_write_words(n->output[2].cpu,pt,4096) ||
       a04_rgx_device_write_words(n->output[3].cpu,pt+512,4096)){status=STATUS_IO_DEVICE_ERROR;goto poison;}
    a04_gpu_dma_barrier();
    for(i=0;i<1024;i+=32){
        if(a04_rgx_device_read_words(seen,(UCHAR *)n->output[2+i/512].cpu+(i%512)*8,sizeof(seen))){status=STATUS_IO_DEVICE_ERROR;goto poison;}
        for(j=0;j<32;j++)if(seen[j]!=pt[i+j]){status=STATUS_IO_DEVICE_ERROR;goto poison;}
    }
    /* Firmware was never started; both PTs now reference only the real MDL.
     * Drop the old private pixel common buffers; retain PD/PT/code/HWRT. */
    a04_vendor_native_storage_free(n,1);status=STATUS_SUCCESS;goto done;
poison:
    n->quarantined=TRUE;r->poisoned=TRUE;
done:
    ExFreePoolWithTag(pt,TAG);return status;
}
NTSTATUS a04_vendor_native_target_before_fw_start(struct a04_vendor_native_cold *n)
{
    if(!n || !n->render || !n->render->firmware || n->render->firmware->hardware_owned ||
       n->quarantined || !n->candidate || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    return n->registered_target?a04_native_mdl_mark_exposed(n->registered_target):STATUS_SUCCESS;
}
