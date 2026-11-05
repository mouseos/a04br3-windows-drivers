/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * AllocateCommonBufferEx contract: Microsoft WDK, Windows 8 and later.
 */
#include "windows_uncached.h"
#include <stddef.h>
NTSTATUS a04_vendor_uncached_allocate(struct a04_vendor_uncached *b,
    PDMA_ADAPTER adapter,ULONG bytes)
{
    PHYSICAL_ADDRESS max;void *cpu;
    if (!b || !adapter || !bytes || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        b->adapter || b->cpu || b->bytes || !adapter->DmaOperations)
        return STATUS_INVALID_PARAMETER;
    if (adapter->DmaOperations->Size <
        offsetof(DMA_OPERATIONS,AllocateCommonBufferEx)+sizeof(adapter->DmaOperations->AllocateCommonBufferEx) ||
        !adapter->DmaOperations->AllocateCommonBufferEx || !adapter->DmaOperations->FreeCommonBuffer)
        return STATUS_NOT_SUPPORTED;
    max.QuadPart=0xffffffffLL;
    cpu=adapter->DmaOperations->AllocateCommonBufferEx(adapter,&max,bytes,&b->logical,FALSE,0);
    if (!cpu) { RtlZeroMemory(b,sizeof(*b));return STATUS_INSUFFICIENT_RESOURCES; }
    if (b->logical.QuadPart<0 || (ULONGLONG)b->logical.QuadPart>0x100000000ULL-bytes ||
        ((ULONG_PTR)cpu&3u)) {
        adapter->DmaOperations->FreeCommonBuffer(adapter,bytes,b->logical,cpu,FALSE);
        RtlZeroMemory(b,sizeof(*b));return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    b->adapter=adapter;b->cpu=cpu;b->bytes=bytes;
    return STATUS_SUCCESS;
}
void a04_vendor_uncached_free(struct a04_vendor_uncached *b)
{
    if (!b || !b->adapter || !b->cpu || !b->bytes || KeGetCurrentIrql()!=PASSIVE_LEVEL) return;
    b->adapter->DmaOperations->FreeCommonBuffer(b->adapter,b->bytes,b->logical,b->cpu,FALSE);
    RtlZeroMemory(b,sizeof(*b));
}
