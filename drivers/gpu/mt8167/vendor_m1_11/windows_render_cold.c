/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Windows ARM32 Device-memory-safe cold preparation for m1.11 contexts.
 * Context construction follows A04BR3RogueRenderResources.h in mouseos/
 * xnu_kernel_benesse_a04br3, f5d43bf1677955e7e367545b9bbbd6449776cdda.
 */
#include "windows_render_cold.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../gpu_mmu.h"
#include "../windows_hw.h"
#define TAG 0x43524741u
static const ULONG sizes[16]={65536u,4096u,4096u,4096u,65536u,4096u,4096u,65536u,4096u,4096u,65536u,4096u,4096u,4096u,4096u,4096u};
NTSTATUS a04_vendor_render_release_cold(struct a04_vendor_render_cold *r)
{
    unsigned i;
    if(!r || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(r->firmware && r->firmware->hardware_owned)return STATUS_DEVICE_BUSY;
    if(r->mapped) {
        uint32_t zeros[16]={0};
        if(!r->firmware || !r->firmware->buffers[0].cpu ||
           a04_rgx_device_write_words((unsigned char *)r->firmware->buffers[0].cpu+
             (A04_VENDOR_RENDER_FW_OFFSET/4096u)*4u,zeros,sizeof(zeros)))
            return STATUS_INVALID_DEVICE_STATE;
        a04_gpu_dma_barrier();r->mapped=FALSE;
    }
    for(i=0;i<16;i++)a04_vendor_uncached_free(&r->buffer[i]);
    RtlZeroMemory(r,sizeof(*r));return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_render_prepare_cold(struct a04_vendor_render_cold *r,
    struct a04_vendor_m1_11_memory *f,PDMA_ADAPTER adapter,BOOLEAN prepare_clear)
{
    void *shadow[17]={0};unsigned i,j;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    struct a04_vendor_render_layout layout={0};
    if(!r || !f || !adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL || r->firmware || r->mapped ||
       !f->staged || !f->uncached || f->hardware_owned || f->adapter!=adapter || !f->buffers[0].cpu)
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<16;i++)if(r->buffer[i].cpu || r->buffer[i].adapter)return STATUS_INVALID_PARAMETER;
    r->firmware=f;
    for(i=0;i<16;i++) {
        if(!prepare_clear && i>=5u && i<=10u)continue;
        status=a04_vendor_uncached_allocate(&r->buffer[i],adapter,sizes[i]);
        if(!NT_SUCCESS(status))goto done;
        if(!a04_rgx_mips_physical_range((uint64_t)r->buffer[i].logical.QuadPart,sizes[i],4096) ||
           ((uintptr_t)r->buffer[i].cpu&7u)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
        for(j=0;j<5;j++)if(a04_rgx_fw_overlap((uint64_t)r->buffer[i].logical.QuadPart,sizes[i],
                f->buffers[j].dma,f->buffers[j].bytes) ||
            a04_rgx_fw_alias(r->buffer[i].cpu,sizes[i],f->buffers[j].cpu,f->buffers[j].bytes)) {
            status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
        }
        for(j=0;j<i;j++)if(r->buffer[j].cpu && (a04_rgx_fw_overlap((uint64_t)r->buffer[i].logical.QuadPart,sizes[i],
                (uint64_t)r->buffer[j].logical.QuadPart,sizes[j]) ||
            a04_rgx_fw_alias(r->buffer[i].cpu,sizes[i],r->buffer[j].cpu,sizes[j]))) {
            status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
        }
        shadow[i]=ExAllocatePoolWithTag(NonPagedPoolNx,sizes[i],TAG);
        if(!shadow[i]){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
        RtlZeroMemory(shadow[i],sizes[i]);
    }
    shadow[16]=ExAllocatePoolWithTag(NonPagedPoolNx,32768u,TAG);
    if(!shadow[16]){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
    if(a04_rgx_device_read_words(shadow[16],f->buffers[0].cpu,32768u) ||
       a04_vendor_render_build(shadow[0],sizes[0],(uint64_t)r->buffer[0].logical.QuadPart,
          (uint64_t)r->buffer[1].logical.QuadPart,A04_VENDOR_RENDER_SCRATCH_VA,1,&layout) ||
       a04_gpu_mmu_map_4k(shadow[1],shadow[2],shadow[3],
          (uint64_t)r->buffer[2].logical.QuadPart,(uint64_t)r->buffer[3].logical.QuadPart,
          A04_VENDOR_RENDER_SCRATCH_VA,(uint64_t)r->buffer[4].logical.QuadPart,16,
          A04_GPU_MMU_SLC_BYPASS) ||
       a04_vendor_render_map_cold(shadow[16],32768u,f->buffers[0].dma,
          shadow[0],(uint64_t)r->buffer[0].logical.QuadPart)) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
    }
    /* Linux m1.11 HWBRN65273: both reserved GPU VAs must point to a
     * zero page in every render address space, including transfer contexts. */
    if(a04_gpu_mmu_map_brn65273(shadow[1],shadow[11],shadow[12],shadow[13],shadow[14],
        (uint64_t)r->buffer[11].logical.QuadPart,(uint64_t)r->buffer[12].logical.QuadPart,
        (uint64_t)r->buffer[13].logical.QuadPart,(uint64_t)r->buffer[14].logical.QuadPart,
        (uint64_t)r->buffer[15].logical.QuadPart)) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
    }
    if(prepare_clear) {
        if(a04_vendor_clear_image_build(&r->clear,A04_VENDOR_RENDER_SCRATCH_VA) ||
           a04_gpu_mmu_map_4k(shadow[1],shadow[5],shadow[6],
               (uint64_t)r->buffer[5].logical.QuadPart,(uint64_t)r->buffer[6].logical.QuadPart,
               UINT64_C(0xa800000000),(uint64_t)r->buffer[7].logical.QuadPart,16,A04_GPU_MMU_SLC_BYPASS) ||
           a04_gpu_mmu_map_4k(shadow[1],shadow[8],shadow[9],
               (uint64_t)r->buffer[8].logical.QuadPart,(uint64_t)r->buffer[9].logical.QuadPart,
               UINT64_C(0xba00000000),(uint64_t)r->buffer[10].logical.QuadPart,16,
               A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_READ_ONLY)) {
            status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
        }
        RtlFillMemory(shadow[4],sizes[4],0xa5);
        RtlCopyMemory(shadow[7],r->clear.pds,sizeof(r->clear.pds));
        RtlCopyMemory(shadow[10],r->clear.usc,sizeof(r->clear.usc));
    }
    for(i=0;i<16;i++)if(shadow[i] && a04_rgx_device_write_words(r->buffer[i].cpu,shadow[i],sizes[i])) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
    }
    /* Publish all owned resources before making the MIPS mapping visible. */
    a04_gpu_dma_barrier();
    if(a04_rgx_device_write_words(f->buffers[0].cpu,shadow[16],32768u)) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
    }
    a04_gpu_dma_barrier();r->mapped=TRUE;r->clear_ready=prepare_clear;r->layout=layout;status=STATUS_SUCCESS;
done:
    for(i=0;i<17;i++)if(shadow[i])ExFreePoolWithTag(shadow[i],TAG);
    if(!NT_SUCCESS(status))(void)a04_vendor_render_release_cold(r);
    return status;
}
