#include "wavert_dma.h"
#include "wavert_cache.h"
NTSTATUS Mt8167AllocateBuffer(PPORTWAVERTSTREAM port, ULONG bytes, Mt8167WaveBuffer *buffer)
{
    PHYSICAL_ADDRESS low, high, physical, next;
    PMDL mdl;
    PVOID mapped;
    ULONG pages, i;
    if (!port || !buffer || buffer->mdl || !bytes || bytes > 1048576) return STATUS_INVALID_PARAMETER;
    /* PortCls allocates pages; WaveRT requires actual >= requested. */
    bytes = (bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    low.QuadPart = 0; high.QuadPart = 0xffffffffULL;
    mdl = port->AllocateContiguousPagesForMdl(low, high, bytes);
    if (!mdl) return STATUS_INSUFFICIENT_RESOURCES;
    pages = port->GetPhysicalPagesCount(mdl);
    if (MmGetMdlByteOffset(mdl) || MmGetMdlByteCount(mdl) < bytes ||
        pages != bytes / PAGE_SIZE) goto invalid;
    physical = port->GetPhysicalPageAddress(mdl, 0);
    if (physical.QuadPart <= 0 || (physical.QuadPart & (PAGE_SIZE - 1)) ||
        (ULONGLONG)physical.QuadPart > 0x100000000ULL - bytes) goto invalid;
    for (i = 1; i < pages; ++i) {
        next = port->GetPhysicalPageAddress(mdl, i);
        if (next.QuadPart < 0 || (ULONGLONG)next.QuadPart != (ULONGLONG)physical.QuadPart + (ULONGLONG)i * PAGE_SIZE) goto invalid;
    }
    mapped = port->MapAllocatedPages(mdl, MT8167_WAVE_CACHE);
    if (!mapped) goto invalid;
    /* No CPU buffer access, including zeroing, precedes the attribute check.
     * MmNonCached can be Device memory on ARM32 and is not an audio ABI. */
    if(Mt8167VerifyWaveCache(mapped,bytes,(ULONG)physical.QuadPart)!=STATUS_SUCCESS) {
        port->UnmapAllocatedPages(mapped,mdl);goto invalid;
    }
    RtlZeroMemory(mapped, bytes);
    buffer->port = port; port->AddRef();
    buffer->mdl = mdl; buffer->mapping = mapped; buffer->bytes = bytes;
    buffer->physicalAddress = (ULONG)physical.QuadPart; buffer->handedToDevice = FALSE;
    return STATUS_SUCCESS;
invalid:
    port->FreePagesFromMdl(mdl);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}
NTSTATUS Mt8167ReleaseUnpublishedBuffer(Mt8167WaveBuffer *buffer)
{
    if (!buffer || !buffer->port || !buffer->mdl || !buffer->mapping) return STATUS_INVALID_PARAMETER;
    if (buffer->handedToDevice) return STATUS_DEVICE_BUSY;
    buffer->port->UnmapAllocatedPages(buffer->mapping, buffer->mdl);
    buffer->port->FreePagesFromMdl(buffer->mdl);
    buffer->port->Release();
    RtlZeroMemory(buffer, sizeof(*buffer));
    return STATUS_SUCCESS;
}

NTSTATUS Mt8167ReleaseStoppedBuffer(Mt8167WaveBuffer *buffer)
{
    if(!buffer || !buffer->port || !buffer->mdl || !buffer->mapping)return STATUS_INVALID_PARAMETER;
    buffer->port->UnmapAllocatedPages(buffer->mapping,buffer->mdl);
    buffer->port->FreePagesFromMdl(buffer->mdl);
    buffer->port->Release();RtlZeroMemory(buffer,sizeof(*buffer));return STATUS_SUCCESS;
}
