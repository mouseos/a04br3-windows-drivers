/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NDIS_RUNTIME_H
#define A04_NDIS_RUNTIME_H
#include <ntddk.h>
#include <ndis.h>
#include "windows_session.h"
#include "windows_irq.h"
#include "station_runtime.h"
#include "native_scan_bridge.h"
#include "native_nbl.h"
#include "native_hif_rx.h"
struct a04_ndis_runtime {
 NDIS_HANDLE adapter,rx_pool;PMDL rx_mdl;PNET_BUFFER_LIST rx_nbl;UCHAR rx_bytes[2304];
 A04_SHARED_WIFI_INTERFACE parent;int referenced,uncertain,paused;
 struct a04_windows_session session;struct a04_windows_irq irq;
 struct a04_station_runtime radio;struct a04_wlan_security security;struct a04_native_tx tx;
 uint8_t mpdu[1532],ethernet[1514];
 NDIS_SPIN_LOCK lock;NDIS_EVENT idle;KTIMER timer;KDPC dpc;
 volatile LONG closing,busy;PNDIS_OID_REQUEST request,next_request;volatile LONG next_cancelled;PNET_BUFFER_LIST send;volatile LONG cancelled;
 DOT11_SSID desired;ULONG filter,lookahead;BOOLEAN power;
};
int a04_ndis_radio_reset(struct a04_ndis_runtime *);
NDIS_STATUS a04_ndis_attributes(struct a04_ndis_runtime *);
NDIS_STATUS a04_ndis_control(struct a04_ndis_runtime *,PNDIS_OID_REQUEST);
void a04_ndis_status(struct a04_ndis_runtime *,NDIS_STATUS,void *,ULONG);
#endif
