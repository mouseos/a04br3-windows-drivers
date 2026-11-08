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
#include "native_security.h"
#include "owned_queue.h"
#include "native_policy.h"
#include "native_retry.h"
#include "checkpoint.h"
#define A04_HOST_STATS_FLAGS (NDIS_STATISTICS_XMIT_OK_SUPPORTED|NDIS_STATISTICS_RCV_OK_SUPPORTED|NDIS_STATISTICS_XMIT_ERROR_SUPPORTED|NDIS_STATISTICS_RCV_ERROR_SUPPORTED|NDIS_STATISTICS_RCV_NO_BUFFER_SUPPORTED|NDIS_STATISTICS_DIRECTED_BYTES_XMIT_SUPPORTED|NDIS_STATISTICS_DIRECTED_FRAMES_XMIT_SUPPORTED|NDIS_STATISTICS_MULTICAST_BYTES_XMIT_SUPPORTED|NDIS_STATISTICS_MULTICAST_FRAMES_XMIT_SUPPORTED|NDIS_STATISTICS_BROADCAST_BYTES_XMIT_SUPPORTED|NDIS_STATISTICS_BROADCAST_FRAMES_XMIT_SUPPORTED|NDIS_STATISTICS_DIRECTED_BYTES_RCV_SUPPORTED|NDIS_STATISTICS_DIRECTED_FRAMES_RCV_SUPPORTED|NDIS_STATISTICS_MULTICAST_BYTES_RCV_SUPPORTED|NDIS_STATISTICS_MULTICAST_FRAMES_RCV_SUPPORTED|NDIS_STATISTICS_BROADCAST_BYTES_RCV_SUPPORTED|NDIS_STATISTICS_BROADCAST_FRAMES_RCV_SUPPORTED|NDIS_STATISTICS_TRANSMIT_QUEUE_LENGTH_SUPPORTED|NDIS_STATISTICS_BYTES_RCV_SUPPORTED|NDIS_STATISTICS_BYTES_XMIT_SUPPORTED|NDIS_STATISTICS_RCV_DISCARDS_SUPPORTED|NDIS_STATISTICS_XMIT_DISCARDS_SUPPORTED|NDIS_STATISTICS_GEN_STATISTICS_SUPPORTED)
struct a04_rx_slot {PMDL mdl;PNET_BUFFER_LIST nbl;ULONG length,epoch;BOOLEAN management;struct a04_native_rx_info info;UCHAR bytes[A04_NATIVE_MPDU_MAX];};
struct a04_runtime_stats {ULONGLONG tx_packets[3],tx_bytes[3],rx_packets[3],rx_bytes[3];ULONGLONG tx_errors,tx_discards,rx_errors,rx_discards,rx_no_buffer;};
struct a04_ndis_runtime {
 struct a04_checkpoint *checkpoint;uint64_t initialize_generation;unsigned runtime_failure_recorded,scan_records;ULONG runtime_last_oid,runtime_last_oid_status;
 NDIS_HANDLE adapter,rx_pool,configuration;struct a04_rx_slot rx[A04_NATIVE_QUEUE];unsigned rx_count;ULONG receive_epoch;UCHAR rx_bytes[A04_NATIVE_MPDU_MAX];
 A04_SHARED_WIFI_INTERFACE parent;int referenced,uncertain,paused;
 struct a04_windows_session session;struct a04_windows_irq irq;
 struct a04_station_runtime radio;struct a04_wlan_security security;struct a04_native_tx tx;struct a04_os_security os_security;
 unsigned association_reported,early_length;uint8_t early_rx[4096];
 uint8_t mpdu[A04_NATIVE_MPDU_MAX],ethernet[A04_NATIVE_ETH_MAX];
 NDIS_SPIN_LOCK lock;NDIS_EVENT idle;KTIMER timer;KDPC dpc;
 volatile LONG closing,busy;PNDIS_OID_REQUEST request,next_request;volatile LONG next_cancelled;PNET_BUFFER_LIST send;struct a04_owned_queue send_queue;unsigned api_refs;volatile LONG cancelled;
 struct a04_runtime_stats stats,association_base;ULONG tx_length,tx_kind;
 DOT11_MAC_ADDRESS excluded[A04_NATIVE_EXCLUDED],multicast[A04_NATIVE_MULTICAST];ULONG excluded_count,multicast_count;
 DOT11_MAC_ADDRESS desired_bssid;BOOLEAN desired_bssid_set,hidden_network,media_streaming,safe_mode;
 UCHAR assoc_ies[600];ULONG assoc_ies_length;DOT11_MAC_ADDRESS assoc_bssid;
 uint64_t association_started;struct a04_peer_loss peer_loss;ULONG power_request;unsigned idle_bss,ps_activity,peer_reset_pending;
 DOT11_SSID desired;ULONG filter,lookahead;BOOLEAN power;
 struct a04_native_retry rx_retry;
};
void a04_ndis_tx_complete(void *,PNET_BUFFER_LIST,NDIS_STATUS);
int a04_ndis_unreachable_check(struct a04_ndis_runtime *);
void a04_ndis_runtime_snapshot(struct a04_ndis_runtime *,ULONG,ULONG,ULONG);
int a04_ndis_poll(struct a04_ndis_runtime *);
int a04_ndis_excluded(const struct a04_ndis_runtime *,const UCHAR *);
NDIS_STATUS a04_ndis_contract_oid(struct a04_ndis_runtime *,PNDIS_OID_REQUEST);
int a04_ndis_rate_lookup(void *,unsigned,unsigned,ULONG *,UCHAR *);
void a04_ndis_peer_disconnect(struct a04_ndis_runtime *,ULONG);
void a04_ndis_association_reported(struct a04_ndis_runtime *);
int a04_ndis_idle_reconcile(struct a04_ndis_runtime *);
int a04_ndis_rf_wake(struct a04_ndis_runtime *);
int a04_ndis_power_profile(struct a04_ndis_runtime *,ULONG,BOOLEAN,int);
int a04_ndis_radio_power(struct a04_ndis_runtime *,int);
int a04_ndis_radio_reset(struct a04_ndis_runtime *);
NDIS_STATUS a04_ndis_attributes(struct a04_ndis_runtime *);
NDIS_STATUS a04_ndis_control(struct a04_ndis_runtime *,PNDIS_OID_REQUEST);
void a04_ndis_status(struct a04_ndis_runtime *,NDIS_STATUS,void *,ULONG);
#endif
