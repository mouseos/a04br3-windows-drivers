/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BT_STATUS_KERNEL_H
#define A04_BT_STATUS_KERNEL_H
#include <ntddk.h>
#include <wdf.h>
#include <bthxddi.h>
#include "../../../tools/bluetooth/mt6630-status.h"
NTSTATUS a04_bt_status_create(WDFDRIVER);
void a04_bt_status_publish(const MT6630_STATUS *);
void a04_bt_status_capabilities(const BTHX_CAPABILITIES *,NTSTATUS);
void a04_bt_status_delete(void);
#endif
