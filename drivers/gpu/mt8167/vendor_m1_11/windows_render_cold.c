/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Windows ARM32 Device-memory-safe cold preparation for m1.11 contexts.
 * Context construction follows A04BR3RogueRenderResources.h in mouseos/
 * xnu_kernel_benesse_a04br3, f5d43bf1677955e7e367545b9bbbd6449776cdda.
 */
#include "windows_render_cold.h"
#include "clear_tile.h"
#ifndef A04_GE8300_VENDOR_TILE_DIAGNOSTIC
#define A04_GE8300_VENDOR_TILE_DIAGNOSTIC 0
#endif
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
    if(r->native_owner || r->copy_owner || (r->firmware && r->firmware->hardware_owned))return STATUS_DEVICE_BUSY;
    if(r->firmware && r->firmware->render_owner!=r)return STATUS_INVALID_DEVICE_STATE;
    if(r->mapped) {
        uint32_t zeros[16]={0};
        if(!r->firmware || !r->firmware->buffers[0].cpu ||
           a04_rgx_device_write_words((unsigned char *)r->firmware->buffers[0].cpu+
             (A04_VENDOR_RENDER_FW_OFFSET/4096u)*4u,zeros,sizeof(zeros)))
            return STATUS_INVALID_DEVICE_STATE;
        a04_gpu_dma_barrier();r->mapped=FALSE;
    }
    for(i=0;i<16;i++)a04_vendor_uncached_free(&r->buffer[i]);
    if(r->firmware && r->firmware->render_owner==r)r->firmware->render_owner=NULL;
    RtlZeroMemory(r,sizeof(*r));return STATUS_SUCCESS;
}
/* Fixed screen-sized, GPU-only diagnostic. Called before firmware start. */
NTSTATUS a04_vendor_render_attach_screen(struct a04_vendor_render_cold *r,
    struct a04_vendor_surface *screen)
{
    uint32_t pc_index,entry=0,source=0;uint64_t first,address,csr;unsigned i,j;
    struct a04_vendor_uncached *resources[7];
    if(!r || !screen || KeGetCurrentIrql()!=PASSIVE_LEVEL || !r->mapped ||
       !r->clear_ready || !r->firmware || r->firmware->hardware_owned || r->screen_surface || r->copy_owner ||
       !screen->prepared || screen->hardware_exposed || screen->gpu_va!=UINT64_C(0x8000000000) ||
       screen->width!=800 || screen->height!=1280 || screen->pitch!=3328 || screen->bytes!=4259840u ||
       !screen->pixels.cpu || !screen->pc.cpu || !r->buffer[1].cpu || !r->buffer[7].cpu)
        return STATUS_INVALID_PARAMETER;
    resources[0]=&screen->pixels;resources[1]=&screen->pc;resources[2]=&screen->pd;
    for(i=0;i<4;i++)resources[3+i]=&screen->pt[i];
    for(i=0;i<7;i++) if(resources[i]->cpu) {
        for(j=0;j<16;j++) if(r->buffer[j].cpu &&
            (a04_rgx_fw_overlap((uint64_t)resources[i]->logical.QuadPart,resources[i]->bytes,
                (uint64_t)r->buffer[j].logical.QuadPart,r->buffer[j].bytes) ||
             a04_rgx_fw_alias(resources[i]->cpu,resources[i]->bytes,r->buffer[j].cpu,r->buffer[j].bytes)))
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        for(j=0;j<5;j++) if(a04_rgx_fw_overlap((uint64_t)resources[i]->logical.QuadPart,resources[i]->bytes,
                r->firmware->buffers[j].dma,r->firmware->buffers[j].bytes) ||
             a04_rgx_fw_alias(resources[i]->cpu,resources[i]->bytes,r->firmware->buffers[j].cpu,r->firmware->buffers[j].bytes))
            return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    pc_index=(uint32_t)(screen->gpu_va>>30);
    if(pc_index>=1024u || a04_rgx_device_read_words(&entry,(UCHAR *)r->buffer[1].cpu+pc_index*4,4) || entry ||
       a04_rgx_device_read_words(&source,(UCHAR *)screen->pc.cpu+pc_index*4,4) ||
       source!=((uint32_t)((uint64_t)screen->pd.logical.QuadPart>>8)|1u)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    first=(uint64_t)1248u*screen->pitch+768u*4u;address=screen->gpu_va+first;
    if(first+31u*(uint64_t)screen->pitch+128u>screen->bytes || (address&127u))return STATUS_INVALID_PARAMETER;
    csr=(UINT64_C(2)<<56)|((UINT64_C(831)&UINT64_C(0x1fffe))<<33)|
        (UINT64_C(1)<<23)|(UINT64_C(2)<<26)|(UINT64_C(3)<<29);
    r->clear.payload[32]=(uint32_t)csr;r->clear.payload[33]=(uint32_t)(csr>>32);
    r->clear.pds[2]=(uint32_t)(address>>2);r->clear.pds[3]=(uint32_t)(address>>34)|(1u<<19);
    for(i=0;i<screen->bytes/4u;i++)((volatile uint32_t *)screen->pixels.cpu)[i]=0xa5a5a5a5u;
    if(a04_rgx_device_write_words(r->buffer[7].cpu,r->clear.pds,sizeof(r->clear.pds)) ||
       a04_rgx_device_write_words((UCHAR *)r->buffer[1].cpu+pc_index*4,&source,4))return STATUS_DEVICE_CONFIGURATION_ERROR;
    a04_gpu_dma_barrier();r->clear_first=(uint32_t)first;r->clear_row_bytes=screen->pitch;
    r->screen_surface=screen;return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_render_prepare_cold(struct a04_vendor_render_cold *r,
    struct a04_vendor_m1_11_memory *f,PDMA_ADAPTER adapter,BOOLEAN prepare_clear,uint32_t clear_colour)
{
    void *shadow[17]={0};unsigned i,j;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    struct a04_vendor_render_layout layout={0};
    if(!r || !f || !adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL || r->firmware || r->mapped || r->native_owner ||
       !f->staged || !f->uncached || f->hardware_owned || f->render_owner || f->adapter!=adapter || !f->buffers[0].cpu)
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<16;i++)if(r->buffer[i].cpu || r->buffer[i].adapter)return STATUS_INVALID_PARAMETER;
    r->firmware=f;f->render_owner=r;
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
        if(a04_vendor_clear_image_build(&r->clear,A04_VENDOR_RENDER_SCRATCH_VA,clear_colour) ||
           a04_gpu_mmu_map_4k(shadow[1],shadow[5],shadow[6],
               (uint64_t)r->buffer[5].logical.QuadPart,(uint64_t)r->buffer[6].logical.QuadPart,
               UINT64_C(0xa800000000),(uint64_t)r->buffer[7].logical.QuadPart,16,A04_GPU_MMU_SLC_BYPASS) ||
           a04_gpu_mmu_map_4k(shadow[1],shadow[8],shadow[9],
               (uint64_t)r->buffer[8].logical.QuadPart,(uint64_t)r->buffer[9].logical.QuadPart,
               UINT64_C(0xba00000000),(uint64_t)r->buffer[10].logical.QuadPart,16,
               A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_READ_ONLY)) {
            status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
        }
#if A04_GE8300_VENDOR_TILE_DIAGNOSTIC
        {
            _Alignas(8) struct a04_vendor_clear_tile tile;
            const struct a04_vendor_clear_tile_request request={128,128,128,64,64};
            if(clear_colour || a04_vendor_clear_tile_build(&tile,&request)) {
                status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
            }
            r->clear=tile.image;r->clear_first=tile.first;r->clear_row_bytes=tile.row_bytes;
        }
#else
        r->clear_first=0;r->clear_row_bytes=128;
#endif
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
