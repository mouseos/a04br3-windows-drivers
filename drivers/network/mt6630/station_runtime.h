/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_STATION_RUNTIME_H
#define A04_STATION_RUNTIME_H
#include "wmt_bridge.h"
#define A04_BSS_MAX 16
struct a04_bss_observation {struct a04_wlan_ap ap;uint8_t ies[768];unsigned ie_length;uint64_t timestamp,host_timestamp;uint8_t frame[1024];unsigned frame_length;};
struct a04_station_runtime {
 struct a04_wmt_bridge *bridge;struct a04_wlan_station station;
 struct a04_wlan_slots slots;struct a04_bss_observation bss[A04_BSS_MAX];unsigned bss_count;
 uint8_t assoc_request[384],assoc_response[1024];unsigned assoc_request_length,assoc_response_length;
 uint8_t selected_bssid[6];unsigned selected_bssid_set;
 unsigned desired_secured;
 int (*exclude_bssid)(void *,const uint8_t *);
 const uint8_t *association_ies;unsigned association_ies_length;
 unsigned rx_pending; /* deferred read-clear RX port bits */
 uint32_t cancelled_scan_sequences[8];
 unsigned initialized,connecting,connected,phase;int failed;
 /* Descriptor-only observations; no MAC/SSID/key material. First rejected AP retained. */
 unsigned rx_phase,rx_bytes,rx_descriptor,rx_error;
 unsigned rejected_aps,rejected_ap_error,rejected_ap_bytes,rejected_ap_descriptor;
 void *context;uint64_t (*now)(void *);uint64_t (*system_time)(void *);void (*wait)(void *,unsigned);
 int (*cancelled)(void *);
 int (*event)(void *,const uint8_t *,unsigned,uint64_t);
 int (*data)(void *,const uint8_t *,unsigned);
 void (*trace)(void *,unsigned,unsigned,unsigned,uint32_t,uint32_t);
};
int a04_station_runtime_init(struct a04_station_runtime *,struct a04_wmt_bridge *);
int a04_station_runtime_poll(struct a04_station_runtime *);
int a04_station_runtime_connect(struct a04_station_runtime *,const uint8_t *,unsigned);
#endif
