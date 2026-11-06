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
 unsigned initialized,connecting,connected,phase;int failed;
 void *context;uint64_t (*now)(void *);void (*wait)(void *,unsigned);
 int (*cancelled)(void *);
 int (*event)(void *,const uint8_t *,unsigned,uint64_t);
 int (*data)(void *,const uint8_t *,unsigned);
};
int a04_station_runtime_init(struct a04_station_runtime *,struct a04_wmt_bridge *);
int a04_station_runtime_poll(struct a04_station_runtime *);
int a04_station_runtime_connect(struct a04_station_runtime *,const uint8_t *,unsigned);
#endif
