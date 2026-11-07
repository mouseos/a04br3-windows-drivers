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
    /* Internal build-local discriminator; old playback owners initialize FALSE. */
    BOOLEAN capture;
    KSRTAUDIO_HWLATENCY captureLatency;
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
    /* Optional PortCls-owned MDL; bind does not grant a DMA mapping. Caller
     * holds allocation until unbind succeeds or the backend is quarantined. */
    NTSTATUS (*bindWaveBuffer)(void *, PMDL, PVOID, ULONG);
    NTSTATUS (*unbindWaveBuffer)(void *, PMDL);
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    /* Returns a WC/verified Normal-NC PortCls MDL, never a common-buffer MDL.
     * Owner retains the allocation across stream instances. Exactly
     * one checked-out client; checkin never releases exposed pages or SG. */
    NTSTATUS (*checkout)(void *,PPORTWAVERTSTREAM,ULONG,PMDL *,PULONG);
    NTSTATUS (*checkin)(void *,PPORTWAVERTSTREAM,PMDL);
    NTSTATUS (*setState)(void *,PPORTWAVERTSTREAM,KSSTATE);
#endif
};
#endif
