/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_WIFI_BOARD_H
#define A04_WINDOWS_WIFI_BOARD_H
#include "../../bus/mt8167-shared/wifi_interface.h"
#include "windows_msdc2.h"
#include "bringup.h"
#include "wmt_bridge.h"
/* PASSIVE_LEVEL, caller serializes PnP and I/O and retains the parent
 * interface reference, exclusive lease and mapped host for this whole call. */
struct a04_windows_wifi_board {
 A04_SHARED_WIFI_INTERFACE *parent;struct a04_windows_msdc2 *host;
 struct a04_wifi_owner *owner;ULONG generation;struct a04_wifi_board_io io;
};
int a04_windows_wifi_board_init(struct a04_windows_wifi_board *,A04_SHARED_WIFI_INTERFACE *,
 struct a04_windows_msdc2 *,struct a04_wifi_owner *,ULONG);
/* This only opens the control transport and queries actual WMT versions.
 * It does not provide firmware, calibration, IRQ handling or NDIS readiness. */
int a04_windows_wifi_control_open(struct a04_windows_wifi_board *,struct a04_wifi_board *,
 struct a04_wifi_life *,struct a04_wmt_bridge *,uint32_t,const struct a04_wifi_life_ops *,
 struct a04_sdio_card_info *);
#endif
