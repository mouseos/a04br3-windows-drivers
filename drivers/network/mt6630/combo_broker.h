/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_COMBO_BROKER_H
#define A04_MT6630_COMBO_BROKER_H
#include "../../bluetooth/mt6630/transport.h"
#include "../../bluetooth/mt6630/broker_abi.h"
struct a04_ndis_runtime;
#define A04_COMBO_RX_SLOTS 32u
struct a04_combo_broker {
 struct a04_bt_transport bt;struct a04_bt_broker_packet rx[A04_COMBO_RX_SLOTS];
 unsigned count;PFILE_OBJECT file;PIRP request;
};
NTSTATUS a04_combo_create(NDIS_HANDLE);
void a04_combo_delete(void);
int a04_combo_publish(struct a04_ndis_runtime *);
void a04_combo_unpublish(struct a04_ndis_runtime *);
void a04_combo_worker(struct a04_ndis_runtime *);
int a04_combo_poll(struct a04_ndis_runtime *);
int a04_combo_stop(struct a04_ndis_runtime *,int removed);
NTSTATUS a04_ndis_combo_enqueue(struct a04_ndis_runtime *,PIRP);
#endif
