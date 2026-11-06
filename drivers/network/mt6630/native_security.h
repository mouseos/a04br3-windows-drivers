/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_SECURITY_H
#define A04_NATIVE_SECURITY_H
#include <ndis.h>
#include <stdint.h>
struct a04_ndis_runtime;
struct a04_native_hif_rx;
/* OS owns PSK/PMK/PTK derivation, EAPOL MIC/replay validation and GTK unwrap.
 * Driver retains only hardware installation metadata and RX packet numbers.
 * Never return key bytes/fingerprints in an OID, log, diagnostic or artifact. */
struct a04_os_key {unsigned valid,seen,wlan;uint8_t fingerprint[32];uint64_t pn[16];};
struct a04_os_security {
 ULONG auth,unicast,multicast,default_id;
 unsigned exclude_unencrypted,port_open,ptk_pending,failed;
 struct a04_os_key ptk,gtk[4];
 unsigned exemption_count;DOT11_PRIVACY_EXEMPTION exemptions[4];
};
NDIS_STATUS a04_os_security_oid(struct a04_ndis_runtime *,PNDIS_OID_REQUEST);
int a04_os_security_event(struct a04_ndis_runtime *,const uint8_t *,unsigned);
NDIS_STATUS a04_os_security_send(struct a04_ndis_runtime *,PNET_BUFFER_LIST,uint64_t,uint64_t);
NDIS_STATUS a04_os_security_receive(struct a04_ndis_runtime *,const uint8_t *,ULONG,struct a04_native_hif_rx *);
void a04_os_security_clear(struct a04_ndis_runtime *);
#endif
