/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_HIF_RX_H
#define A04_NATIVE_HIF_RX_H
#include "native_receive.h"
/* The caller provides the actual advertised PHY/rate table mapping; no rate
 * index is guessed. `allowed_channels` is a verified 2.4 GHz channel mask. */
typedef int (*a04_native_rate_lookup)(void *,unsigned hw_mode,unsigned hw_rate,ULONG *phy,UCHAR *rate);
struct a04_native_hif_rx {const uint8_t *mpdu;ULONG length;struct a04_native_rx_info info;unsigned cipher,key,wlan,tid,header;uint64_t pn;};
/* Strict open, directed/broadcast/multicast, unfragmented, non-QoS FromDS MPDU subset. No Ethernet header
 * translation, protected data, aggregated frames or unobserved metadata. */
NDIS_STATUS a04_native_hif_rx_management(const uint8_t *,ULONG,unsigned,void *,a04_native_rate_lookup,struct a04_native_hif_rx *);
NDIS_STATUS a04_native_hif_rx_decode(const uint8_t *,ULONG,unsigned allowed_channels,
 const uint8_t local[6],const uint8_t ap[6],void *,a04_native_rate_lookup,struct a04_native_hif_rx *);
/* Raw plaintext LLC/SNAP from verified CCMP hardware metadata; caller must
 * enforce installed key/controlled-port/PN, association retry ownership and
 * normalize the MAC header. Decode alone never indicates a packet. */
NDIS_STATUS a04_native_hif_rx_decode_security(const uint8_t *,ULONG,unsigned,
 const uint8_t [6],const uint8_t [6],void *,a04_native_rate_lookup,struct a04_native_hif_rx *);
NDIS_STATUS a04_native_hif_rx_indicate(NDIS_HANDLE,PNET_BUFFER_LIST,const uint8_t *,ULONG,unsigned,
 const uint8_t local[6],const uint8_t ap[6],void *,a04_native_rate_lookup);
#endif
