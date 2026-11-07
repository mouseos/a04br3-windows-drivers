/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BT_HFP_ENDPOINT_H
#define A04_BT_HFP_ENDPOINT_H
#include <ntddk.h>
#include "sideband.h"
#ifdef __cplusplus
extern "C" {
#endif
#define A04_BT_HFP_PROVIDER_VERSION 2u
enum {A04_BT_HFP_SPEAKER=0,A04_BT_HFP_MIC=1};
struct a04_bt_hfp_buffer {PMDL mdl;ULONG bytes,generation;MEMORY_CACHING_TYPE cache;};
struct a04_bt_hfp_latency {ULONG fifo,chipset,codec;};
/* New BT-private provider contract, NOT a layout/cast of Audio v11.
 * Provider is supplied by the real Audio/IRQ/DMA owner; there is no default.
 * All callbacks except reference/dereference, buffer_live, position and fault
 * run at PASSIVE_LEVEL. Those five callbacks must be safe <=DISPATCH_LEVEL.
 * One context is one physical combo/duplex ring pair, independent of names/MAC.
 */
struct a04_bt_hfp_provider {
 ULONG size,version;PVOID context;
 void (*reference)(PVOID);void (*dereference)(PVOID);
 /* Must prove this live HFP stack belongs to the leased physical MT6630.
  * GUID membership or a name match alone is not sufficient. No user IDs.
  * Return STATUS_NOT_SUPPORTED for unrelated radios (no fault/rundown). */
 NTSTATUS (*match_hfp_target)(PVOID,PDEVICE_OBJECT);
 struct a04_bt_sideband_ops sideband;
 /* Verify actual GORM/FC72 + controller PCM/master-clock configuration and
  * paired combo ownership for this WinBlue narrowband profile (CVSD=0), 8k/1/16.
  * codec_id identifies the requested profile, not a GET_CODEC_ID response;
  * GET_CODEC_ID is Windows 10+, not a WDK8.1 DDI.
  * A caller flag, a version reply or record checksum alone is not this proof. */
 NTSTATUS (*validate_pcm)(PVOID,UCHAR codec_id,ULONG rate,ULONG channels,ULONG bits);
 /* Exact client/DMA common buffer, no separate tone/mock shadow mapping.
  * A real nonzero generation, coherent noncached MDL and 2-byte frames.
  * Page-aligned dedicated pages: zero every mapped page before publication,
  * no unrelated pool data; MDL byte count equals page-multiple buffer bytes.
  * Failure without an MDL guarantees no allocation/ownership side effects;
  * a partial failure must return the retained MDL for owner rundown.
  * Duplex prepare must retain real rings for both directions; an unused
  * direction may be physically clocked silence/discard, never simulated DMA. */
 NTSTATUS (*allocate)(PVOID,ULONG direction,ULONG requested,struct a04_bt_hfp_buffer *);
 BOOLEAN (*buffer_live)(PVOID,ULONG direction,ULONG generation);
 /* Independently stop/drain accesses to THIS MDL before releasing it, even
  * when the peer pin is running. Failure retains the allocation and context. */
 NTSTATUS (*free_drained)(PVOID,ULONG direction,const struct a04_bt_hfp_buffer *);
 /* prepare/start_dma clocks duplex with both client rings initially inactive.
  * active=TRUE binds/resumes this allocated ring; FALSE stops accesses to it
  * and drains IRQ/cache users before return. Keep peer clocks and ring live.
  * An inactive leg uses physical silence/discard owned by the provider. */
 NTSTATUS (*set_pin_active)(PVOID,ULONG direction,BOOLEAN active);
 NTSTATUS (*position)(PVOID,ULONG direction,ULONG generation,ULONGLONG *play,ULONGLONG *write);
 NTSTATUS (*device_description)(PVOID,PDEVICE_DESCRIPTION);
 /* KSRTAUDIO_HWLATENCY units: FIFO bytes, chipset/codec 100-ns delays. */
 NTSTATUS (*latency)(PVOID,ULONG direction,struct a04_bt_hfp_latency *);
 /* Queue real passive owner rundown on HFP/stream/IRQ failure. Never perform
  * PIO or wait in a notification completion; keep mappings/module refs pinned. */
 void (*fault)(PVOID,NTSTATUS);
 /* Queue this cleanup asynchronously at PASSIVE only after actual owner
  * rundown. Pin module/context until callback returns; do not run inline.
  * One deferred cleanup attempt; failure is retained, not a retry loop. */
 NTSTATUS (*retire)(PVOID,PWORKER_THREAD_ROUTINE,PVOID);
};
struct a04_bt_hfp_metadata {GUID input_category,output_category,container;BOOLEAN volume,nrec;ULONG volume_values_size;};
struct a04_bt_hfp_session;
/* Runtime metadata only: no name/address is returned or logged. */
NTSTATUS a04_bt_hfp_metadata(struct a04_bt_hfp_session *,struct a04_bt_hfp_metadata *);
/* Latest real completed HFP status while notifications run; otherwise perform
 * the fixed standard immediate query. Request-connect acceptance is not data. */
NTSTATUS a04_bt_hfp_connection(struct a04_bt_hfp_session *,BOOLEAN *);
/* Fixed HFP service-level operation on the already matched target only.
 * Completion is request acceptance, NOT a connected/disconnected indication.
 * Real connection state continues to come from the status update IOCTL. */
NTSTATUS a04_bt_hfp_request_connection(struct a04_bt_hfp_session *,BOOLEAN connect);
/* BT-private control/event ABI; provider v2 layout stays unchanged.
 * Changes are real HFP completion values. Callback is PASSIVE, nonblocking,
 * and must not call this session or synchronously run provider teardown. */
enum {A04_BT_HFP_CHANGE_CONNECTION=1,A04_BT_HFP_CHANGE_VOLUME=2};
struct a04_bt_hfp_observer;
struct a04_bt_hfp_volume_range {LONG minimum,maximum;ULONG step,member_flags;};
NTSTATUS a04_bt_hfp_volume_range(struct a04_bt_hfp_session *,ULONG values_size,struct a04_bt_hfp_volume_range *);
NTSTATUS a04_bt_hfp_volume_get(struct a04_bt_hfp_session *,ULONG direction,LONG *);
NTSTATUS a04_bt_hfp_volume_set(struct a04_bt_hfp_session *,ULONG direction,LONG);
NTSTATUS a04_bt_hfp_events_start(struct a04_bt_hfp_session *,BOOLEAN volume);
NTSTATUS a04_bt_hfp_events_stop(struct a04_bt_hfp_session *);
NTSTATUS a04_bt_hfp_observe(struct a04_bt_hfp_session *,ULONG direction,
 void (*changed)(PVOID,ULONG),PVOID,struct a04_bt_hfp_observer **);
NTSTATUS a04_bt_hfp_unobserve(struct a04_bt_hfp_session *,struct a04_bt_hfp_observer *);
/* Kernel only: link must currently enumerate under the Microsoft HFP bypass
 * interface GUID. No user interface accepts a path or arbitrary IOCTL. */
NTSTATUS a04_bt_hfp_attach(PDEVICE_OBJECT owner,PCUNICODE_STRING interface_link,
 const struct a04_bt_hfp_provider *,struct a04_bt_hfp_session **);
/* BT-private normal COM final-release deferral (not provider fault/retire).
 * Queue once <=DISPATCH, invoke at PASSIVE, pin session/FDO/provider until
 * callback returns. No public IOCTL/callback/path broker. */
NTSTATUS a04_bt_hfp_defer_cleanup(struct a04_bt_hfp_session *,PWORKER_THREAD_ROUTINE,PVOID);
void a04_bt_hfp_reference(struct a04_bt_hfp_session *);
void a04_bt_hfp_dereference(struct a04_bt_hfp_session *);
NTSTATUS a04_bt_hfp_detach(struct a04_bt_hfp_session *); /* stop first; caller owns creator reference */
NTSTATUS a04_bt_hfp_pin_claim(struct a04_bt_hfp_session *,ULONG direction);
void a04_bt_hfp_pin_release(struct a04_bt_hfp_session *,ULONG direction);
NTSTATUS a04_bt_hfp_run(struct a04_bt_hfp_session *,ULONG direction);
NTSTATUS a04_bt_hfp_suspend(struct a04_bt_hfp_session *,ULONG direction);
NTSTATUS a04_bt_hfp_status(struct a04_bt_hfp_session *); /* <=DISPATCH, no I/O */
const struct a04_bt_hfp_provider *a04_bt_hfp_provider(struct a04_bt_hfp_session *);
NTSTATUS a04_bt_hfp_register_notifications(PDRIVER_OBJECT,PDRIVER_NOTIFICATION_CALLBACK_ROUTINE,PVOID,PVOID *);
NTSTATUS a04_bt_hfp_unregister_notifications(PVOID);
#ifdef __cplusplus
}
#endif
#endif
