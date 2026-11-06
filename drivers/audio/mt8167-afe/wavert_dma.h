#ifndef MT8167_WAVERT_DMA_H
#define MT8167_WAVERT_DMA_H
#include <portcls.h>
/* PASSIVE_LEVEL; caller serializes and holds a reference to the port stream.
 * The physical address is not proven to be a device DMA address.
 * Never release after handing to hardware unless a platform-specific drain
 * proof has succeeded. The miniport uses this PortCls allocation; a physical
 * address must never be substituted for a DMA adapter logical address. */
struct Mt8167WaveBuffer {
    PPORTWAVERTSTREAM port;
    PMDL mdl;
    PVOID mapping;
    ULONG bytes;
    ULONG physicalAddress;
    BOOLEAN handedToDevice; /* sticky: never clear on a live allocation */
};
NTSTATUS Mt8167AllocateBuffer(PPORTWAVERTSTREAM port, ULONG bytes, Mt8167WaveBuffer *buffer);
/* Only for a buffer that was never exposed to the device. */
NTSTATUS Mt8167ReleaseUnpublishedBuffer(Mt8167WaveBuffer *buffer);
#endif
