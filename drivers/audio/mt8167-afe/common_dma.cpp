#include "common_dma.h"
static void Mt8167DmaBarrier()
{
#if defined(__clang__) && defined(_ARM_)
    __asm__ volatile ("" ::: "memory");
    __builtin_arm_dmb(15);
    __asm__ volatile ("" ::: "memory");
#else
    KeMemoryBarrier();
#endif
}
NTSTATUS Mt8167CommonAllocate(PDMA_ADAPTER adapter, ULONG size, Mt8167CommonDma *out)
{
    PHYSICAL_ADDRESS logical;
    PVOID cpu;
    PMDL mdl;
    if (!adapter || !adapter->DmaOperations || !adapter->DmaOperations->AllocateCommonBuffer ||
        !adapter->DmaOperations->FreeCommonBuffer || !out || out->cpu || out->mdl ||
        size < PAGE_SIZE || size > 1048576 || (size & (PAGE_SIZE - 1)))
        return STATUS_INVALID_PARAMETER;
    logical.QuadPart = 0;
    cpu = adapter->DmaOperations->AllocateCommonBuffer(adapter, size, &logical, FALSE);
    if (!cpu) return STATUS_INSUFFICIENT_RESOURCES;
    if (logical.QuadPart <= 0 || (logical.QuadPart & (PAGE_SIZE - 1)) ||
        (ULONGLONG)logical.QuadPart > 0x100000000ULL - size) goto invalid;
    mdl = IoAllocateMdl(cpu, size, FALSE, FALSE, NULL);
    if (!mdl) goto invalid;
    MmBuildMdlForNonPagedPool(mdl);
    RtlZeroMemory(cpu, size);
    Mt8167DmaBarrier();
    out->adapter = adapter; out->cpu = cpu; out->mdl = mdl;
    out->logical = logical; out->size = size; out->prepared = FALSE; out->exposed = FALSE;
    return STATUS_SUCCESS;
invalid:
    adapter->DmaOperations->FreeCommonBuffer(adapter, size, logical, cpu, FALSE);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}
NTSTATUS Mt8167CommonPrepareForDevice(Mt8167CommonDma *buffer)
{
    if (!buffer || !buffer->cpu || !buffer->mdl || !buffer->adapter || buffer->exposed)
        return STATUS_INVALID_PARAMETER;
    /* Device reads audio: ReadOperation=FALSE. This only prepares initial
     * contents. Live engine PCM additionally requires both mappings to be
     * MmWriteCombined/verified Normal-NC and the SG address to identify
     * these exact PortCls MDL pages. No common buffer is mapped to AudioSrv. */
    KeFlushIoBuffers(buffer->mdl, FALSE, TRUE);
    Mt8167DmaBarrier();
    buffer->prepared = TRUE;
    return STATUS_SUCCESS;
}
NTSTATUS Mt8167CommonMarkExposed(Mt8167CommonDma *buffer)
{
    if (!buffer || !buffer->cpu || !buffer->mdl || !buffer->adapter || !buffer->prepared || buffer->exposed)
        return STATUS_INVALID_PARAMETER;
    /* Barrier is ordering only, not proof of platform-specific DMA coherency. */
    Mt8167DmaBarrier();
    buffer->exposed = TRUE;
    return STATUS_SUCCESS;
}
NTSTATUS Mt8167CommonReleaseUnexposed(Mt8167CommonDma *buffer)
{
    if (!buffer || !buffer->adapter || !buffer->cpu || !buffer->mdl) return STATUS_INVALID_PARAMETER;
    if (buffer->exposed) return STATUS_DEVICE_BUSY;
    IoFreeMdl(buffer->mdl);
    buffer->adapter->DmaOperations->FreeCommonBuffer(buffer->adapter, buffer->size,
                                                    buffer->logical, buffer->cpu, FALSE);
    RtlZeroMemory(buffer, sizeof(*buffer));
    return STATUS_SUCCESS;
}
