/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_WIFI_DIAGNOSTIC_H
#define A04_WINDOWS_WIFI_DIAGNOSTIC_H
#include "windows_board.h"
#include "windows_firmware.h"
/* PASSIVE_LEVEL under one non-reentrant PnP/IO lock. The caller owns the
 * parent reference, a real lease and mapped host, including IRQ rundown.
 * A failure after entry retains all resources for explicit recovery. */
enum a04_diag_state { A04_DIAG_OFF, A04_DIAG_CONTROL, A04_DIAG_READY, A04_DIAG_FAILED, A04_DIAG_STOPPED };
struct a04_windows_diagnostic {
 enum a04_diag_state state;
 struct a04_windows_wifi_board board_io;
 struct a04_wifi_board board;
 struct a04_wifi_life life;
 struct a04_sdio_card_info card;
 struct a04_wmt_bridge bridge;
};
int a04_windows_diagnostic_start(struct a04_windows_diagnostic *, A04_SHARED_WIFI_INTERFACE *,
 struct a04_windows_msdc2 *, struct a04_wifi_owner *, ULONG, uint32_t,
 const struct a04_wifi_life_ops *, const uint8_t [32], const struct a04_wmt_startup_options *);
int a04_windows_diagnostic_stop(struct a04_windows_diagnostic *);
#endif
