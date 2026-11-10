/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_DISPLAY_TRANSPORT356_H
#define A04_DISPLAY_TRANSPORT356_H
#include <ntddk.h>
#include "panel_timing.h"
#include "../wddm-render/frontend_memory.h"
/* PnP device interface, not a named control device or legacy GPU lease.
 * DIS0 owns OVL and its DMA adapter; one render miniport owns POST/VidPn/GPU.
 * The lifetime of every function pointer is covered by Header references. */
static const GUID A04DisplayTransport356Guid={0x552c3560,0x7418,0x4e34,{0xb2,0x0a,0x81,0x67,0x35,0x60,0x10,0x01}};
#define A04_DISPLAY_TRANSPORT_VERSION 1u
struct a04_display_transport_snapshot {
 ULONG size,version,ready,width,height,pitch,format,active_address,post_address;
 ULONGLONG frame_serial;ULONG last_status,retained_maps,controller_idle,restore_completed;
};
typedef void (*a04_display_frame_notify)(void *,ULONGLONG serial);
struct a04_display_transport_v1 {
 INTERFACE Header;
 NTSTATUS (*BrightnessResources)(void *,PCM_RESOURCE_LIST *); /* loan covered by Header reference */
 NTSTATUS (*Snapshot)(void *,struct a04_display_transport_snapshot *);
 /* A retained actual VidMm Normal-RAM lease. It must cover native GPU PFNs;
  * no MmMapIoSpace/POST MDL, fabricated SG, copied frame, or user pointer. */
 NTSTATUS (*Map)(void *,const struct geo_frontend_mdl_lease *,void **cookie);
 NTSTATUS (*Unmap)(void *,void *cookie); /* only after display AND GPU retirement */
 NTSTATUS (*Select)(void *,void *cookie); /* PASSIVE mode-change; real frame IRQ required */
 NTSTATUS (*Restore)(void *); /* closes new scanout and proves OVL DMA idle first */
 NTSTATUS (*Subscribe)(void *,a04_display_frame_notify,void *);
 NTSTATUS (*Unsubscribe)(void *);
 NTSTATUS (*Raster)(void *,ULONG *line,BOOLEAN *in_blank);
 void (*EmergencyRestore)(void *); /* best-effort only; never frees/quiesces mappings */
};
/* v1 layout and request remain available to old consumers. v2 is required
 * by the timed unified driver; a v1 provider must not silently satisfy it. */
struct a04_display_transport_v2 {
 struct a04_display_transport_v1 v1;
 NTSTATUS (*Timing)(void *,struct a04_panel_timing *);
};
#endif
