/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_DOD_TQ_INTERFACE_H
#define GEO_DOD_TQ_INTERFACE_H
#include <ntddk.h>
/* A kernel-only bounce copy, not a VidPn source, scanout lease, scheduling
 * fence, NoDMA receipt or a second GPU owner. Private GUIDs, versioned ABI. */
static const GUID GeoDodTqDeviceGuid={0x539cd696,0x847c,0x4494,{0xb1,0xe6,0xd0,0xe4,0x03,0x37,0xd5,0x17}};
static const GUID GeoDodTqQueryGuid={0x917ef67c,0xfbbc,0x4f9d,{0x93,0x10,0xb5,0x98,0xee,0xf8,0xa9,0x46}};
#define GEO_DOD_TQ_VERSION 1u
#define GEO_DOD_TQ_TILE_BYTES 4096u
#define GEO_DOD_TQ_PRIVATE_BOUNCE 1u
#define GEO_DOD_TQ_CPU_VISIBLE 1u
struct geo_dod_tq_receipt {
 ULONG size,version,flags,reserved;
 ULONGLONG epoch;
 ULONG task_sequence,flush_sequence;
};
/* PASSIVE_LEVEL/APCs enabled. Exactly one packed32x32 BGRA tile in kernel
 * nonpaged RAM. No physical address, MDL, GPU packet or POST pointer crosses
 * this interface. Backend stages into its OWN DMA buffers before submission;
 * it must NEVER retain either caller pointer, including after timeout.
 * SUCCESS requires actual TQ completion/SLC/GP/CPU readback evidence. Errors
 * retain uncertain private GPU backing in the existing owner, never in DOD.
 * This narrow contract permits CPU redraw on failure without racing POST DMA. */
typedef NTSTATUS (*GEO_DOD_TQ_COPY)(PVOID,const VOID *,ULONG,VOID *,struct geo_dod_tq_receipt *);
struct geo_dod_tq_interface {
 INTERFACE Header;
 ULONG capabilities,reserved;
 GEO_DOD_TQ_COPY CopyTile;
};
static __inline BOOLEAN GeoDodTqReceiptValid(const struct geo_dod_tq_receipt *r)
{
 return r && r->size==sizeof(*r) && r->version==GEO_DOD_TQ_VERSION &&
  r->flags==GEO_DOD_TQ_CPU_VISIBLE && !r->reserved && r->epoch &&
  r->task_sequence && r->flush_sequence;
}
#endif
