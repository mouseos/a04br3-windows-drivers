/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_stage.h"
#include "../windows_hw.h"
_Static_assert(sizeof(RGXFWIF_INIT)==592 && offsetof(RGXFWIF_INIT,bFirmwareStarted)==260,"m1.11 init ABI");
_Static_assert(sizeof(RGXFWIF_TRACEBUF)==4152 && sizeof(RGXFWIF_HWRINFOBUF)==2072,"m1.11 trace ABI");
_Static_assert(sizeof(RGXFWIF_RUNTIME_CFG)==24 && sizeof(RGXFWIF_OS_CONFIG)==20,"m1.11 config ABI");
_Static_assert(sizeof(RGXFWIF_KCCB_CMD)==56 && sizeof(RGXFWIF_FWCCB_CMD)==40 && sizeof(RGXFWIF_CCB_CTL)==16,"m1.11 CCB ABI");
static const ULONG sizes[5]={A04_VENDOR_TABLE_BYTES,A04_VENDOR_CODE_BYTES,A04_VENDOR_DATA_BYTES,A04_RGX_MAIN_BYTES,A04_RGX_CONFIG_BYTES};
/* Allocate extra space to align the device address of the 32 KiB table. */
static const ULONG allocated[5]={2*A04_VENDOR_TABLE_BYTES,A04_VENDOR_CODE_BYTES,A04_VENDOR_DATA_BYTES,A04_RGX_MAIN_BYTES,A04_RGX_CONFIG_BYTES};
NTSTATUS a04_vendor_m1_11_windows_free(struct a04_vendor_m1_11_memory *m)
{
    unsigned i;
    if(!m || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    if(m->hardware_owned) return STATUS_DEVICE_BUSY;
    if(m->adapter && (!m->adapter->DmaOperations || !m->adapter->DmaOperations->FreeCommonBuffer))
        return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<5;i++) if(m->allocation[i]) {
        if(!m->adapter) return STATUS_INVALID_DEVICE_STATE;
        m->adapter->DmaOperations->FreeCommonBuffer(m->adapter,allocated[i],m->logical[i],m->allocation[i],FALSE);
        m->allocation[i]=NULL;
    }
    RtlZeroMemory(m,sizeof(*m)); return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_m1_11_windows_stage(struct a04_vendor_m1_11_memory *m,
    PDMA_ADAPTER adapter,const void *image,size_t bytes)
{
    unsigned i,j; NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(!m || !adapter || !image || KeGetCurrentIrql()!=PASSIVE_LEVEL || m->adapter || m->hardware_owned ||
       m->buffers[0].cpu || m->buffers[1].cpu || m->buffers[2].cpu || m->buffers[3].cpu || m->buffers[4].cpu ||
       m->allocation[0] || m->allocation[1] || m->allocation[2] || m->allocation[3] || m->allocation[4] || !adapter->DmaOperations ||
       !adapter->DmaOperations->AllocateCommonBuffer || !adapter->DmaOperations->FreeCommonBuffer)
        return STATUS_INVALID_PARAMETER;
    m->adapter=adapter;
    for(i=0;i<5;i++) {
        uint64_t address,offset;
        m->allocation[i]=adapter->DmaOperations->AllocateCommonBuffer(adapter,allocated[i],&m->logical[i],FALSE);
        if(!m->allocation[i]) { status=STATUS_INSUFFICIENT_RESOURCES; goto fail; }
        address=(uint64_t)m->logical[i].QuadPart;
        offset=i ? 0 : ((0u-address)&(A04_VENDOR_TABLE_BYTES-1u));
        if(address>UINT64_MAX-offset || (uintptr_t)m->allocation[i]>UINTPTR_MAX-(uintptr_t)offset) goto fail;
        m->buffers[i].cpu=(unsigned char *)m->allocation[i]+(size_t)offset;
        m->buffers[i].dma=address+offset; m->buffers[i].bytes=sizes[i];
    }
    /* Validate all ranges, including runtime allocations, before any write. */
    for(i=0;i<5;i++) {
        if(!a04_rgx_fw_pointer(m->buffers[i].cpu,sizes[i]) ||
           !a04_rgx_mips_physical_range(m->buffers[i].dma,sizes[i],i?4096:A04_VENDOR_TABLE_BYTES) ||
           a04_rgx_fw_alias(image,bytes,m->buffers[i].cpu,sizes[i]) ||
           a04_rgx_fw_overlap(m->buffers[i].dma,sizes[i],0x1fc00000u,0x4000u)) goto fail;
        for(j=0;j<i;j++) if(a04_rgx_fw_alias(m->buffers[i].cpu,sizes[i],m->buffers[j].cpu,sizes[j]) ||
           a04_rgx_fw_overlap(m->buffers[i].dma,sizes[i],m->buffers[j].dma,sizes[j])) goto fail;
    }
    if(a04_vendor_m1_11_stage(image,bytes,m->buffers)) goto fail;
    if(a04_rgx_fw_runtime_init(&m->runtime,m->buffers[3].cpu,m->buffers[3].dma,
            m->buffers[4].cpu,m->buffers[4].dma,26000000u) ||
       a04_rgx_fw_runtime_map((uint32_t *)m->buffers[0].cpu,A04_VENDOR_TABLE_BYTES/4,
            m->buffers[3].dma,m->buffers[4].dma)) goto fail;
    /* DDK allocation grants GPU write access to firmware code. */
    for(i=0;i<A04_VENDOR_CODE_BYTES/4096;i++) ((uint32_t *)m->buffers[0].cpu)[i]|=4u;
    for(i=1;i<5;i++) { status=a04_gpu_dma_sync(m->buffers[i].cpu,sizes[i],FALSE); if(!NT_SUCCESS(status)) goto fail; }
    status=a04_gpu_dma_sync(m->buffers[0].cpu,sizes[0],FALSE); if(!NT_SUCCESS(status)) goto fail;
    a04_gpu_dma_barrier();m->staged=TRUE;return STATUS_SUCCESS;
fail:
    (void)a04_vendor_m1_11_windows_free(m);return status;
}
