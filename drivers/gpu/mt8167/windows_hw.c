/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Windows ARM32 GE8300 register and DMA helpers. The register split order is
 * from the board GPL source m1.8ED4333936/services/server/include/osfunc.h.
 * Neither resource acquisition nor firmware startup is performed here. */
#include "windows_hw.h"
#define PVR_REGISTER_SIZE 0x80000u
#define PVR_TABLE_SIZE 0x4000u
static int valid_address(PHYSICAL_ADDRESS address, ULONG bytes)
{
    return address.QuadPart>=0 && (address.QuadPart & 4095)==0 &&
        (ULONGLONG)address.QuadPart <= 0x100000000ULL-bytes;
}
void a04_gpu_free_memory(struct a04_gpu_dma_memory *m)
{
    if (!m || !m->adapter) return;
    if (m->table) m->adapter->DmaOperations->FreeCommonBuffer(m->adapter,PVR_TABLE_SIZE,m->table_logical,m->table,FALSE);
    if (m->heap) m->adapter->DmaOperations->FreeCommonBuffer(m->adapter,A04_PVR_HEAP_SIZE,m->heap_logical,m->heap,FALSE);
    RtlZeroMemory(m,sizeof(*m));
}
NTSTATUS a04_gpu_allocate_memory(PDMA_ADAPTER adapter, struct a04_gpu_dma_memory *m)
{
    if (!adapter || !m || !adapter->DmaOperations || !adapter->DmaOperations->AllocateCommonBuffer ||
        !adapter->DmaOperations->FreeCommonBuffer || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(m,sizeof(*m)); m->adapter=adapter;
    m->heap=adapter->DmaOperations->AllocateCommonBuffer(adapter,A04_PVR_HEAP_SIZE,&m->heap_logical,FALSE);
    if (!m->heap || !valid_address(m->heap_logical,A04_PVR_HEAP_SIZE)) goto fail;
    m->table=adapter->DmaOperations->AllocateCommonBuffer(adapter,PVR_TABLE_SIZE,&m->table_logical,FALSE);
    if (!m->table || !valid_address(m->table_logical,PVR_TABLE_SIZE)) goto fail;
    if ((ULONGLONG)m->heap_logical.QuadPart < (ULONGLONG)m->table_logical.QuadPart+PVR_TABLE_SIZE &&
        (ULONGLONG)m->table_logical.QuadPart < (ULONGLONG)m->heap_logical.QuadPart+A04_PVR_HEAP_SIZE) goto fail;
    return STATUS_SUCCESS;
fail:
    a04_gpu_free_memory(m); return STATUS_INSUFFICIENT_RESOURCES;
}
static uint32_t read32(void *arg,uint32_t offset)
{
    struct a04_gpu_register_context *c=arg;
    return READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->base+offset));
}
static uint64_t read64(void *arg,uint32_t offset)
{
    uint32_t high=read32(arg,offset+4), low=read32(arg,offset);
    return ((uint64_t)high<<32)|low;
}
static void write32(void *arg,uint32_t offset,uint32_t value)
{
    struct a04_gpu_register_context *c=arg;
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(c->base+offset),value);
}
static void write64(void *arg,uint32_t offset,uint64_t value)
{
    write32(arg,offset,(uint32_t)value); write32(arg,offset+4,(uint32_t)(value>>32));
}
static void delay_us(void *arg,unsigned usec) { UNREFERENCED_PARAMETER(arg); KeStallExecutionProcessor(usec); }
NTSTATUS a04_gpu_register_io_init(struct a04_gpu_register_context *context,
        volatile void *base, ULONG length, struct a04_pvr_register_io *io)
{
    if (!context || !base || !io || length<PVR_REGISTER_SIZE || ((ULONG_PTR)base&3u)) return STATUS_INVALID_PARAMETER;
    context->base=base; context->length=length;
    io->context=context; io->read32=read32; io->read64=read64;
    io->write32=write32; io->write64=write64; io->delay_us=delay_us;
    return STATUS_SUCCESS;
}
void a04_gpu_dma_barrier(void) { KeMemoryBarrier(); }
