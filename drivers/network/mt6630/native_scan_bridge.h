/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_SCAN_BRIDGE_H
#define A04_NATIVE_SCAN_BRIDGE_H
#include "native_scan.h"
#include "wmt_bridge.h"
/* The Windows adapter supplies these from a verified regulatory policy and
 * initialized/calibrated firmware. Channels are (band,number) pairs. */
struct a04_native_scan_policy {
 const UCHAR *hidden_ssid;ULONG hidden_ssid_length;
 unsigned bss,sequence,probes,min_dwell,dwell,timeout,channel_count;
 UCHAR channels[108];
};
/* Only from a serialized PASSIVE_LEVEL worker, not directly at DISPATCH_LEVEL.
 * Success means the real command was submitted; SCAN_CONFIRM follows only a
 * matching firmware event. This is not yet connected to a MiniportOidRequest. */
NDIS_STATUS a04_native_scan_start(struct a04_wmt_bridge *,const void *,ULONG,
 const struct a04_native_scan_policy *,ULONG *needed);
/* Called after a matching real SCAN_DONE was accepted by the bridge. */
NDIS_STATUS a04_native_scan_indicate(struct a04_wmt_bridge *,NDIS_HANDLE);
/* Timeout, cancellation or I/O error; never reuse the poisoned scan session. */
NDIS_STATUS a04_native_scan_fail(struct a04_wmt_bridge *,NDIS_HANDLE,NDIS_STATUS);
#endif
