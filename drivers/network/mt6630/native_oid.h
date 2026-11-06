/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OID_H
#define A04_NATIVE_OID_H
#include "native_scan_bridge.h"
/* The adapter must keep bridge and policy alive, serialize this with PnP and
 * the event worker, and call it on a PASSIVE_LEVEL work item. Only explicit
 * scan OIDs are implemented; all others return NOT_SUPPORTED. */
struct a04_native_oid_context {struct a04_wmt_bridge *bridge;struct a04_native_scan_policy policy;};
NDIS_STATUS a04_native_oid_dispatch(struct a04_native_oid_context *,PNDIS_OID_REQUEST);
#endif
