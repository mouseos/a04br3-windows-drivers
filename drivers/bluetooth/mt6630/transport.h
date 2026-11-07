/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_BT_TRANSPORT_H
#define A04_MT6630_BT_TRANSPORT_H
#include "../../network/mt6630/wmt_bridge.h"
/* One PASSIVE_LEVEL serialized owner of function 2, after WMT bootstrap.
 * Callbacks borrow one complete HCI packet without H4 type until return.
 * No packet bytes (including addresses/keys) are logged here. */
#define A04_BT_MAX_HCI 1998u
struct a04_bt_version { uint8_t hci_version,lmp_version;uint16_t hci_revision,manufacturer,lmp_subversion; };
struct a04_bt_transport {
 struct a04_wmt_bridge *bridge;
 uint64_t (*now_us)(void *); /* monotonic, includes time spent in SDIO */
 void *context;int (*receive)(void *,unsigned,const uint8_t *,unsigned);
 uint8_t tx[2560],rx[2560],hci[A04_BT_MAX_HCI+1],wmt[2000];
 unsigned pending,hci_used,hci_expected,wmt_used,wmt_expected;
 unsigned awaiting,complete,func_wait,func_complete,enabled,ready;
 uint8_t reply[256];unsigned reply_length;int failed;
 unsigned version_sent,version_complete,version_status,version_length,version_valid;
 struct a04_bt_version version;
};
int a04_bt_open(struct a04_bt_transport *,struct a04_wmt_bridge *,void *,int (*)(void *,unsigned,const uint8_t *,unsigned),uint64_t (*)(void *));
int a04_bt_poll(struct a04_bt_transport *);
int a04_bt_write(struct a04_bt_transport *,unsigned,const uint8_t *,unsigned);
/* Caller supplies checksum-verified original runtime record prefix (36 bytes).
 * This function does not accept defaults or extract files from the user. */
int a04_bt_initialize(struct a04_bt_transport *,const uint8_t [36]);
int a04_bt_disable(struct a04_bt_transport *);
#endif
