/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_SCAN_H
#define A04_NATIVE_SCAN_H
#include <ndis.h>
#include "native_limits.h"
/* Stable CPU snapshot suitable for a PASSIVE_LEVEL work item. No pointers
 * into the NDIS OID buffer remain after this function returns. */
struct a04_native_scan_request {
 ULONG scan_type,forced,ssid_length,ie_length;
 UCHAR ssid[32],ie[600];
 ULONG ssid_count,ssid_lengths[A04_NATIVE_SCAN_SSIDS];
 UCHAR ssids[A04_NATIVE_SCAN_SSIDS][32];
};
/* A strict subset: wildcard BSSID, up to four SSIDs, no explicit PHY list,
 * restricted scan or 802.11d request IDs. Channel/regulatory policy and dwell
 * are supplied by the runtime, never invented from the NDIS request. */
NDIS_STATUS a04_native_scan_parse(const void *,ULONG,struct a04_native_scan_request *,ULONG *bytes_needed);
#endif
