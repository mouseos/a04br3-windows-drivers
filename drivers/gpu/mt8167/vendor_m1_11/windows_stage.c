/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_stage.h"
#include "../windows_hw.h"
static const ULONG sizes[3]={A04_VENDOR_TABLE_BYTES,A04_VENDOR_CODE_BYTES,A04_VENDOR_DATA_BYTES};
/* Allocate extra space to align the device address of the 32 KiB table. */
static const ULONG allocated[3]={2*A04_VENDOR_TABLE_BYTES,A04_VENDOR_CODE_BYTES,A04_VENDOR_DATA_BYTES};
NTSTATUS a04_vendor_m1_11_windows_free(struct a04_vendor_m1_11_memory *m)
{
    unsigned i;
    if(!m || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    if(m->hardware_owned) return STATUS_DEVICE_BUSY;
    if(m->adapter && (!m->adapter->DmaOperations || !m->adapter->DmaOperations->FreeCommonBuffer))
        return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<3;i++) if(m->allocation[i]) {
        if(!m->adapter) return STATUS_INVALID_DEVICE_STATE;
        m->adapter->DmaOperations->FreeCommonBuffer(m->adapter,allocated[i],m->logical[i],m->allocation[i],FALSE);
        m->allocation[i]=NULL;
    }
    RtlZeroMemory(m,sizeof(*m)); return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_m1_11_windows_stage(struct a04_vendor_m1_11_memory *m,
    PDMA_ADAPTER adapter,const void *image,size_t bytes)
{
    unsigned i; NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(!m || !adapter || !image || KeGetCurrentIrql()!=PASSIVE_LEVEL || m->adapter || m->hardware_owned ||
       m->buffers[0].cpu || m->buffers[1].cpu || m->buffers[2].cpu ||
       m->allocation[0] || m->allocation[1] || m->allocation[2] || !adapter->DmaOperations ||
       !adapter->DmaOperations->AllocateCommonBuffer || !adapter->DmaOperations->FreeCommonBuffer)
        return STATUS_INVALID_PARAMETER;
    m->adapter=adapter;
    for(i=0;i<3;i++) {
        uint64_t address,offset;
        m->allocation[i]=adapter->DmaOperations->AllocateCommonBuffer(adapter,allocated[i],&m->logical[i],FALSE);
        if(!m->allocation[i]) { status=STATUS_INSUFFICIENT_RESOURCES; goto fail; }
        address=(uint64_t)m->logical[i].QuadPart;
        offset=i ? 0 : ((0u-address)&(A04_VENDOR_TABLE_BYTES-1u));
        if(address>UINT64_MAX-offset || (uintptr_t)m->allocation[i]>UINTPTR_MAX-(uintptr_t)offset) goto fail;
        m->buffers[i].cpu=(unsigned char *)m->allocation[i]+(size_t)offset;
        m->buffers[i].dma=address+offset; m->buffers[i].bytes=sizes[i];
    }
    if(a04_vendor_m1_11_stage(image,bytes,m->buffers)) goto fail;
    for(i=1;i<3;i++) { status=a04_gpu_dma_sync(m->buffers[i].cpu,sizes[i],FALSE); if(!NT_SUCCESS(status)) goto fail; }
    status=a04_gpu_dma_sync(m->buffers[0].cpu,sizes[0],FALSE); if(!NT_SUCCESS(status)) goto fail;
    a04_gpu_dma_barrier();m->staged=TRUE;return STATUS_SUCCESS;
fail:
    (void)a04_vendor_m1_11_windows_free(m);return status;
}
