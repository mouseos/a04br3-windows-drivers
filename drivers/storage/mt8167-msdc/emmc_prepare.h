/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_EMMC_PREPARE_H
#define A04_EMMC_PREPARE_H
#include "board_owner.h"
#include "pinmux.h"
#include "cold_init.h"
#include "host_prepare.h"
#include "../../bus/mt8167-shared/vemc_restore.h"
/* Offline sequencing prototype. PASSIVE_LEVEL, not a WDF or Storport API.
 * Every callback must refer to the same exclusive non-revocable parent lease;
 * acquire holds a PASSIVE_LEVEL-capable parent lock through all callbacks,
 * which must not reenter that lock. The WDF parent now uses a passive wait lock, but no current Windows parent
 * implements this full lease contract. A software
 * lease cannot protect an MMIO instruction from physical removal. */
struct a04_emmc_prepare_ops {
    const struct a04_board_ops *board;
    const struct a04_vemc_io *vemc;
    const struct a04_pinmux_ops *pins;
    const struct a04_cold_authority *cold;
    const struct a04_msdc_io *host;
    int (*reset_and_prepare_host)(void *,uint32_t);
    /* When supplied, use the concrete host sequence instead of the callback. */
    const struct a04_host_prepare_ops *host_prepare;
};
struct a04_emmc_prepare_state {
    struct a04_board_owner board;
    struct a04_vemc_state vemc;
    struct a04_pinmux_state pins;
    struct a04_host_prepare_state host;
};
enum a04_emmc_prepare_result { A04_PREPARE_OBSERVED,A04_PREPARE_INVALID,A04_PREPARE_CLAIM,
    A04_PREPARE_RAIL,A04_PREPARE_PIN,A04_PREPARE_CLOCK,A04_PREPARE_HOST,A04_PREPARE_CARD };
/* Even OBSERVED must not open the boot/resume gate: returned power_owned and
 * clock_stable are zero, and IRQ/PnP and real power evidence remain missing.
 * Any failure after claim poisons and retains the lease; no implicit rollback.
 * success also retains the lease. No card data is written. */
enum a04_emmc_prepare_result a04_emmc_prepare(struct a04_emmc_prepare_state *,
    const struct a04_emmc_prepare_ops *,unsigned selector,unsigned limit,uint8_t scratch[512],
    struct a04_resume_observation *out);
#endif
