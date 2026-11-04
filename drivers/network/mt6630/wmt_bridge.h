/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_WMT_BRIDGE_H
#define A04_WINDOWS_WMT_BRIDGE_H
#include "lifetime.h"
/* Isolate names that overlap with the Windows host's own transport. */
#define a04_sdio_transport a04_xnu_sdio_transport
#define a04_sdio_transfer a04_xnu_sdio_transfer
#include "vendor_xnu/A04BR3WMTDownload.h"
#include "vendor_xnu/A04BR3WLANFirmware.h"
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
 unsigned identity_ready,wmt_ready,wlan_ready,failed;uint32_t pending_wlan_status;
};
int a04_wmt_bridge_open(struct a04_wmt_bridge *,struct a04_wifi_life *,unsigned functions,void *,void (*)(void *,unsigned));
/* Data is a stable, owned snapshot with a provenance-verified SHA-256; input
 * memory stays pinned for the entire call. No firmware bytes are embedded. */
int a04_wmt_bridge_boot_wmt(struct a04_wmt_bridge *,const void *,unsigned,
 const uint8_t expected_sha256[32],int (*sha256)(const uint8_t *,size_t,uint8_t[32]),
 const struct a04_wmt_startup_options *);
int a04_wmt_bridge_boot_wlan(struct a04_wmt_bridge *,const void *,unsigned,int (*sha256)(const uint8_t *,size_t,uint8_t[32]));
/* Single serialized read-clear status consumer, not an ISR implementation. */
int a04_wmt_bridge_poll_wlan_status(struct a04_wmt_bridge *,uint32_t *);
/* Receive the indicated FIFO packet while status is pending; returned view is
 * valid only until the next receive or teardown. Caller parses the packet. */
int a04_wmt_bridge_receive(struct a04_wmt_bridge *,unsigned port,unsigned polls,
 const uint8_t **data,unsigned *length);
/* Call only after the consumer actually handled every asserted status bit. */
int a04_wmt_bridge_ack_wlan_status(struct a04_wmt_bridge *,uint32_t handled);
#endif
