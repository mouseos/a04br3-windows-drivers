/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OID_ASYNC_H
#define A04_NATIVE_OID_ASYNC_H
#include "native_oid.h"
/* One outstanding normal OID. Driver must stop new callbacks before close,
 * retain backend/adapter until close succeeds and NDIS has completed OIDs,
 * and serialize worker access with IRQ/D0/other SDIO users separately. */
struct a04_native_oid_async {
 NDIS_HANDLE adapter;
 struct a04_native_oid_context *oid;
 NDIS_SPIN_LOCK lock;
 NDIS_EVENT idle;
 PNDIS_OID_REQUEST request;
 unsigned busy,started,cancelled,closing,initialized;
};
NDIS_STATUS a04_native_oid_async_init(struct a04_native_oid_async *,NDIS_HANDLE,struct a04_native_oid_context *);
NDIS_STATUS a04_native_oid_async_submit(struct a04_native_oid_async *,PNDIS_OID_REQUEST);
/* Best effort: a matching queued request is cancelled. Once synchronous SDIO
 * work has begun it is not interrupted or rolled back by this callback. */
void a04_native_oid_async_cancel(struct a04_native_oid_async *,PVOID request_id);
/* PASSIVE_LEVEL only after caller closes OID entry/rundown. On timeout the
 * object remains allocated and closing; never destroy while work is pending. */
NDIS_STATUS a04_native_oid_async_close(struct a04_native_oid_async *,unsigned timeout_ms);
#endif
