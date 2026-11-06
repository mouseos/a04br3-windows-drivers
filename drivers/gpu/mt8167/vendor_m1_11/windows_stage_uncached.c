/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_stage.h"
#include "windows_clock.h"
#include "windows_uncached.h"
#include "A04BR3RogueRuntimeRebase.h"
#include "A04BR3RogueDeviceWords.h"
#include "../windows_hw.h"
static const ULONG sizes[5]={A04_VENDOR_TABLE_BYTES,A04_VENDOR_CODE_BYTES,A04_VENDOR_DATA_BYTES,A04_RGX_MAIN_BYTES,A04_RGX_CONFIG_BYTES};
static const ULONG alloc_sizes[5]={2*A04_VENDOR_TABLE_BYTES,A04_VENDOR_CODE_BYTES,A04_VENDOR_DATA_BYTES,A04_RGX_MAIN_BYTES,A04_RGX_CONFIG_BYTES};
NTSTATUS a04_vendor_m1_11_windows_stage_uncached(struct a04_vendor_m1_11_memory *m,
    PDMA_ADAPTER adapter,const void *image,size_t bytes)
{
    struct a04_vendor_buffer shadow[5]={0};struct a04_vendor_uncached allocation={0};
    IMG_PUINT32 trace_host;RGXFWIF_TRACEBUF *shadow_trace;
    unsigned i,j;ULONG step=1;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if (!m || !adapter || !image || KeGetCurrentIrql()!=PASSIVE_LEVEL || m->adapter ||
        m->runtime || m->staged || m->uncached || m->hardware_owned || m->render_owner) return STATUS_INVALID_PARAMETER;
    for (i=0;i<5;i++) if(m->allocation[i] || m->buffers[i].cpu) return STATUS_INVALID_PARAMETER;
    status=mt8167_gpu_clock_read(&m->clock_hz,&m->clock_parent);
    if(!NT_SUCCESS(status)) return status;
    m->adapter=adapter;
    step=2;
    m->runtime=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*m->runtime),0x72564741u);
    if(!m->runtime) { status=STATUS_INSUFFICIENT_RESOURCES;goto done; }
    for(i=0;i<5;i++) {
        uint64_t address,offset;
        step=10+i;
        status=a04_vendor_uncached_allocate(&allocation,adapter,alloc_sizes[i]);
        if(!NT_SUCCESS(status)) goto done;
        m->allocation[i]=allocation.cpu;m->logical[i]=allocation.logical;
        RtlZeroMemory(&allocation,sizeof(allocation));
        address=(uint64_t)m->logical[i].QuadPart;
        offset=i ? 0 : ((0u-address)&(A04_VENDOR_TABLE_BYTES-1u));
        if(address>UINT64_MAX-offset || (uintptr_t)m->allocation[i]>UINTPTR_MAX-(uintptr_t)offset) {
            status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
        }
        m->buffers[i].cpu=(unsigned char *)m->allocation[i]+(size_t)offset;
        m->buffers[i].dma=address+offset;m->buffers[i].bytes=sizes[i];
        shadow[i].cpu=ExAllocatePoolWithTag(NonPagedPoolNx,sizes[i],0x73464741u);
        if(!shadow[i].cpu) {status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
        shadow[i].dma=m->buffers[i].dma;shadow[i].bytes=sizes[i];
    }
    step=20;
    for(i=0;i<5;i++) {
        if (!a04_rgx_fw_pointer(m->buffers[i].cpu,sizes[i]) || ((uintptr_t)m->buffers[i].cpu&7u) ||
            !a04_rgx_mips_physical_range(m->buffers[i].dma,sizes[i],i?4096:A04_VENDOR_TABLE_BYTES) ||
            a04_rgx_fw_alias(image,bytes,m->buffers[i].cpu,sizes[i]) ||
            a04_rgx_fw_overlap(m->buffers[i].dma,sizes[i],0x1fc00000u,0x4000u)) goto bad;
        for(j=0;j<i;j++) if(a04_rgx_fw_alias(m->buffers[i].cpu,sizes[i],m->buffers[j].cpu,sizes[j]) ||
            a04_rgx_fw_overlap(m->buffers[i].dma,sizes[i],m->buffers[j].dma,sizes[j])) goto bad;
    }
    step=21;if(a04_vendor_m1_11_stage(image,bytes,shadow)) goto bad;
    step=22;if(a04_rgx_fw_runtime_init(m->runtime,shadow[3].cpu,shadow[3].dma,shadow[4].cpu,shadow[4].dma,m->clock_hz)) goto bad;
    step=23;if(a04_rgx_fw_runtime_map((uint32_t *)shadow[0].cpu,A04_VENDOR_TABLE_BYTES/4,shadow[3].dma,shadow[4].dma)) goto bad;
    for(i=0;i<A04_VENDOR_CODE_BYTES/4096;i++) ((uint32_t *)shadow[0].cpu)[i]|=4u;
    shadow_trace=m->runtime->trace;
    step=24;if(a04_rgx_runtime_rebase(m->runtime,m->buffers[3].cpu,m->buffers[4].cpu,&trace_host)) goto bad;
    shadow_trace->sTraceBuf[0].pui32TraceBuffer=trace_host;
    step=25;for(i=0;i<5;i++) if(a04_rgx_device_write_words(m->buffers[i].cpu,shadow[i].cpu,sizes[i])) goto bad;
    a04_gpu_dma_barrier();m->uncached=TRUE;m->staged=TRUE;status=STATUS_SUCCESS;goto done;
bad:
    status=STATUS_DEVICE_CONFIGURATION_ERROR;
done:
    for(i=0;i<5;i++) if(shadow[i].cpu) ExFreePoolWithTag(shadow[i].cpu,0x73464741u);
    if(!NT_SUCCESS(status)) (void)a04_vendor_m1_11_windows_free(m);
    m->stage_step=step;
    return status;
}
