#ifndef MT8167_STREAM_BACKEND_H
#define MT8167_STREAM_BACKEND_H
#include <portcls.h>
/* Internal kernel-only interface. PnP owner supplies a live, referenced
 * backend; it owns DMA, synchronization, IRQ, clocks and shutdown. Callbacks
 * except position are PASSIVE_LEVEL and serialized by the adapter. Position
 * may run at DISPATCH_LEVEL concurrently with stop/removal: its implementation
 * and data must be nonpaged, nonblocking, and synchronized with the ISR and
 * teardown. Failed or ambiguous hardware stop must retain all resources.
 * Failure from allocate
 * must clean up its own partial allocation. No provider exists yet. */
struct Mt8167StreamBackend {
    void *context;
    void (*reference)(void *);
    void (*dereference)(void *);
    NTSTATUS (*allocate)(void *, ULONG, PMDL *, PULONG, PULONG, MEMORY_CACHING_TYPE *);
    void (*freeBuffer)(void *, PMDL, ULONG);
    /* On entry *possiblyExposed is TRUE. Set FALSE only when failure is
     * known to precede every DMA-address publication and hardware start;
     * success or uncertain failure must leave it TRUE. */
    NTSTATUS (*start)(void *,BOOLEAN *possiblyExposed);
    NTSTATUS (*stop)(void *);
    NTSTATUS (*position)(void *, PKSAUDIO_POSITION);
    NTSTATUS (*getDeviceDescription)(void *, PDEVICE_DESCRIPTION);
};
#endif
