/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_KMD_CLEAR_H
#define GEO_KMD_CLEAR_H
#ifndef DXGKDDI_INTERFACE_VERSION
#define DXGKDDI_INTERFACE_VERSION 0x4002
#endif
#include <ntddk.h>
#include <dispmprt.h>
#include "clear_engine.h"
#include "aperture.h"
#include "frontend_memory.h"
#include "frontend/draw_link_abi.h"
#include "frontend/texture_draw_abi.h"
#include "frontend/shader_draw_abi.h"
struct geo_mdl_record;
struct geo_pending_payload {struct geo_clear_packet clear;struct geo_draw_packet *draw;UINT generation;};
/* These are DDI implementation modules, not a DriverEntry that registers an
 * incomplete WDDM adapter. The full miniport owns synchronization/rundown,
 * querycaps, paging and real DIRQL ISR/DPC routing. */
struct geo_owner_session;
struct geo_kmd_device;
struct geo_kmd_adapter {
 struct geo_owner_session *session;
 struct geo_kmd_device *devices;
 UINT device_count;
 struct geo_aperture_config aperture;
 UINT aperture_ready;
 DXGKCB_GETHANDLEDATA get_handle_data;
 struct geo_engine engine;
 struct geo_native_draw_ops_v1 draw;
 /* Must use the provider's real interrupt synchronization object. No manual
  * fabricated DIRQL and no sleeping callback at interrupt IRQL. */
 BOOLEAN (*synchronize)(void *owner,PKSYNCHRONIZE_ROUTINE routine,void *argument);
 void *sync_context;
 /* Set once at StartDevice after real IRQ ownership is established. Retain
  * through callback rundown; never replace these from a user IOCTL. */
 HANDLE dxgk_device;
 DXGKCB_NOTIFY_INTERRUPT notify_interrupt;
 DXGKCB_QUEUE_DPC queue_dpc;
 DXGKCB_NOTIFY_DPC notify_dpc;
 KIRQL owner_irql;
 LONG dpc_pending,service_pending;
 UINT notifications_ready;
 struct geo_frontend_memory memory_api;
 struct geo_mdl_record *memory_records;
 KMUTEX memory_lock;
 LONG memory_record_count,memory_fault;
 UINT memory_ready;
 struct geo_native_draw_ops_v3 draw_v3;
 struct geo_native_draw_ops_v4 draw_v4;
 struct geo_native_draw_ops_v2 draw_v2; /* append: preserve original field offsets */
 struct geo_pending_payload pending[GEO_MAX_JOBS];
};
struct geo_kmd_context { struct geo_kmd_adapter *adapter;struct geo_kmd_device *device;struct geo_kmd_context *next;HANDLE runtime; };
/* Full miniport must call completion only from its real registered ISR,
 * holding the same owner interrupt lock as submit. IRQL equality is an
 * additional check, not a proof that the caller really came from that ISR. */
NTSTATUS GeoInitializeClearNotifications(struct geo_kmd_adapter *,const DXGKRNL_INTERFACE *,KIRQL actual_owner_irql);
NTSTATUS GeoCompleteFromInterrupt(struct geo_kmd_adapter *,uint64_t job,uint32_t generation,int32_t status,uint32_t visibility);
void GeoCollectPendingPayloads(struct geo_kmd_adapter *);
void GeoClearDpc(struct geo_kmd_adapter *);
DXGKDDI_CREATEALLOCATION GeoCreateAllocation;
DXGKDDI_DESTROYALLOCATION GeoDestroyAllocation;
DXGKDDI_BUILDPAGINGBUFFER GeoBuildPagingBuffer;
DXGKDDI_RENDER GeoRender;
DXGKDDI_PATCH GeoPatch;
DXGKDDI_SUBMITCOMMAND GeoSubmitCommand;
DXGKDDI_QUERYCURRENTFENCE GeoQueryCurrentFence;
DXGKDDI_PREEMPTCOMMAND GeoPreemptCommand;
DXGKDDI_CANCELCOMMAND GeoCancelCommand;
void GeoNotifyReadyPreemption(struct geo_kmd_adapter *);
#endif
