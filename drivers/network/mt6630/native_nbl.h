/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_NBL_H
#define A04_NATIVE_NBL_H
#include <ndis.h>
#include "wmt_bridge.h"
/* One outstanding NBL, exactly one NET_BUFFER, serialized by the caller.
 * PASSIVE_LEVEL only. Caller retains adapter, station/security and nonpaged
 * scratch for each synchronous call; NBL remains owned until completion. */
struct a04_native_tx {
 NDIS_HANDLE adapter;
 PNET_BUFFER_LIST pending;
 struct a04_wlan_data_state state;
 void *completion_context;void (*completion)(void *,PNET_BUFFER_LIST,NDIS_STATUS);
};
NDIS_STATUS a04_native_nbl_send(struct a04_native_tx *,struct a04_wmt_bridge *,
 const struct a04_wlan_station *,struct a04_wlan_security *,PNET_BUFFER_LIST,
 uint8_t *mpdu,size_t mpdu_capacity,uint8_t *ethernet,size_t ethernet_capacity,
 a04_wlan_mic_verify,void *,uint64_t now,uint64_t deadline);
/* Completes only upon a matching firmware TX event or a reported error.
 * Return status is event dispatch; an air rejection can return SUCCESS
 * with FAILURE delivered to the NBL completion callback. */
NDIS_STATUS a04_native_nbl_tx_event(struct a04_native_tx *,struct a04_wmt_bridge *,
 const struct a04_wlan_station *,struct a04_wlan_security *,const uint8_t *,unsigned,uint64_t now);
/* Caller has stopped and drained all event producers before abort. */
NDIS_STATUS a04_native_nbl_abort(struct a04_native_tx *,struct a04_wmt_bridge *);
#endif
