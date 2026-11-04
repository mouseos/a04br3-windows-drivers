/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_WMT_BRIDGE_H
#define A04_WINDOWS_WMT_BRIDGE_H
#include "lifetime.h"
/* Isolate names that overlap with the Windows host's own transport. */
#define a04_sdio_transport a04_xnu_sdio_transport
#define a04_sdio_transfer a04_xnu_sdio_transfer
#include "vendor_xnu/A04BR3WMTDownload.h"
#include "vendor_xnu/A04BR3WLANFirmware.h"
#include "vendor_xnu/A04BR3WLANProtocol.h"
#include "vendor_xnu/A04BR3WLANData.h"
#undef a04_sdio_transport
#undef a04_sdio_transfer
/* Caller allocates this large context outside the kernel stack and serializes
 * it with the shared SDIO lifetime. No ISR or competing WMT/WLAN status reader.
 * Context must be zero-initialized. The delay callback must run at
 * PASSIVE_LEVEL and be bounded. */
struct a04_wmt_bridge {
 struct a04_wifi_life *life;
 void *delay_context;void (*delay)(void *,unsigned);
 struct a04_sdio_io io;struct a04_xnu_sdio_transport transport;
 struct a04_wmt_channel channel;struct a04_wmt_identity identity;
 struct a04_wmt_firmware_set wmt_fw;struct a04_wmt_download_state wmt_state;
 struct a04_wlan_host wlan;struct a04_wlan_firmware wlan_fw;struct a04_wlan_download_state wlan_state;
 struct a04_wlan_scan_state scan;
 unsigned identity_ready,wmt_ready,wlan_ready,failed;uint32_t pending_wlan_status;
};
int a04_wmt_bridge_open(struct a04_wmt_bridge *,struct a04_wifi_life *,unsigned functions,void *,void (*)(void *,unsigned));
/* Data is a stable, owned snapshot with a provenance-verified SHA-256; input
 * memory stays pinned for the entire call. No firmware bytes are embedded. */
int a04_wmt_bridge_boot_wmt(struct a04_wmt_bridge *,const void *,unsigned,
 const uint8_t expected_sha256[32],int (*sha256)(const uint8_t *,size_t,uint8_t[32]),
 const struct a04_wmt_startup_options *);
int a04_wmt_bridge_boot_wlan(struct a04_wmt_bridge *,const void *,unsigned,int (*sha256)(const uint8_t *,size_t,uint8_t[32]));
/* Scan policy (channels, timing and calibration) must come from an actual
 * configured regulatory/runtime owner. Completion follows a real event. */
int a04_wmt_bridge_scan_begin(struct a04_wmt_bridge *,const struct a04_wlan_scan *);
int a04_wmt_bridge_scan_event(struct a04_wmt_bridge *,const uint8_t *,unsigned);
int a04_wmt_bridge_scan_consume(struct a04_wmt_bridge *);
/* Timeout/cancellation poisons the session: a late SCAN_DONE is ambiguous. */
int a04_wmt_bridge_scan_fail(struct a04_wmt_bridge *,int reason);
/* Association and key calls require a real calibrated runtime and a real
 * supplicant. Callers retain state and serialize each event with SDIO. */
int a04_wmt_bridge_station_setup(struct a04_wmt_bridge *,struct a04_wlan_station *,const struct a04_wlan_local *);
int a04_wmt_bridge_station_select(struct a04_wmt_bridge *,struct a04_wlan_station *,const struct a04_wlan_ap *,unsigned,unsigned,unsigned,uint64_t,uint64_t);
int a04_wmt_bridge_station_event(struct a04_wmt_bridge *,struct a04_wlan_station *,const uint8_t *,unsigned,uint64_t);
int a04_wmt_bridge_station_auth(struct a04_wmt_bridge *,struct a04_wlan_station *,uint64_t);
int a04_wmt_bridge_station_auth_rx(struct a04_wmt_bridge *,struct a04_wlan_station *,const uint8_t *,unsigned,uint64_t);
int a04_wmt_bridge_station_assoc(struct a04_wmt_bridge *,struct a04_wlan_station *,unsigned,unsigned,const uint8_t *,unsigned,uint64_t);
int a04_wmt_bridge_station_assoc_rx(struct a04_wmt_bridge *,struct a04_wlan_station *,const uint8_t *,unsigned,unsigned,uint64_t);
int a04_wmt_bridge_security_begin(struct a04_wmt_bridge *,struct a04_wlan_security *,const struct a04_wlan_station *,uint64_t,uint64_t);
int a04_wmt_bridge_security_rx(struct a04_wmt_bridge *,struct a04_wlan_security *,const uint8_t *,unsigned,a04_wlan_mic_verify,void *,uint64_t);
int a04_wmt_bridge_key_install(struct a04_wmt_bridge *,struct a04_wlan_security *,struct a04_wlan_station *,unsigned,unsigned,const uint8_t [16],const uint8_t [8],uint64_t);
int a04_wmt_bridge_key_event(struct a04_wmt_bridge *,struct a04_wlan_security *,const struct a04_wlan_station *,const uint8_t *,unsigned,uint64_t);
int a04_wmt_bridge_authorize(struct a04_wmt_bridge *,struct a04_wlan_security *,int,uint64_t);
/* Station/security state is owned and serialized by the real association and
 * key manager; no link, keys, MIC result or firmware completion is synthesized. */
int a04_wmt_bridge_tx_ethernet(struct a04_wmt_bridge *,const struct a04_wlan_station *,
 struct a04_wlan_security *,struct a04_wlan_data_state *,const uint8_t *,unsigned,
 a04_wlan_mic_verify,void *,uint64_t,uint64_t);
/* Caller owns disjoint scratch and supplies a validated associated station. */
int a04_wmt_bridge_tx_mpdu(struct a04_wmt_bridge *,const struct a04_wlan_station *,
 struct a04_wlan_security *,struct a04_wlan_data_state *,const uint8_t *,unsigned,
 uint8_t *,size_t,a04_wlan_mic_verify,void *,uint64_t,uint64_t);
int a04_wmt_bridge_rx_ethernet(struct a04_wmt_bridge *,const struct a04_wlan_station *,
 struct a04_wlan_security *,const uint8_t *,unsigned,struct a04_wlan_ethernet *);
int a04_wmt_bridge_tx_event(struct a04_wmt_bridge *,const struct a04_wlan_station *,
 struct a04_wlan_security *,struct a04_wlan_data_state *,const uint8_t *,unsigned,uint64_t);
/* Single serialized read-clear status consumer, not an ISR implementation. */
int a04_wmt_bridge_poll_wlan_status(struct a04_wmt_bridge *,uint32_t *);
/* Receive the indicated FIFO packet while status is pending; returned view is
 * valid only until the next receive or teardown. Caller parses the packet. */
int a04_wmt_bridge_receive(struct a04_wmt_bridge *,unsigned port,unsigned polls,
 const uint8_t **data,unsigned *length);
/* Call only after the consumer actually handled every asserted status bit. */
int a04_wmt_bridge_ack_wlan_status(struct a04_wmt_bridge *,uint32_t handled);
#endif
