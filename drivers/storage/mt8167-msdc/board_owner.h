/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC_BOARD_OWNER_H
#define A04_MSDC_BOARD_OWNER_H
#include <stdint.h>
#include "../../input/a04br3-buttons/pmic_read.h"
/* PASSIVE_LEVEL-only prototype. The parent must already implement an exclusive
 * lifetime lease for PWRAP, TOPCKGEN and GPIO, including firmware and all other
 * clients; callbacks cannot reenter. No Windows parent currently provides it.
 * Pin mode alone does not prove pull, drive, supply voltage or card readiness. */
struct a04_board_ops {
    void *context;
    int (*acquire)(void *,uint32_t generation);
    int (*owned)(void *,uint32_t generation);
    int (*read_rails)(void *,struct a04_emmc_rails_raw *);
    int (*read_pin_mode)(void *,unsigned pin,unsigned *mode);
    int (*read_clock)(void *,unsigned offset,uint32_t *value);
    int (*write_clock)(void *,unsigned offset,uint32_t value);
    int (*quiescent)(void *);
    int (*release)(void *,uint32_t generation);
};
struct a04_board_owner { uint32_t generation; unsigned active, poisoned, prepared; };
enum a04_board_result { A04_BOARD_OK, A04_BOARD_INVALID, A04_BOARD_BUSY,
    A04_BOARD_OWNER_LOST, A04_BOARD_RAILS, A04_BOARD_PINS, A04_BOARD_CLOCK,
    A04_BOARD_NOT_QUIESCENT };
/* Split form permits rail and pin preparation under the retained lease.
 * The caller must poison and retain it on any intermediate failure; release
 * also refuses while the split operation is still incomplete. */
enum a04_board_result a04_board_begin(struct a04_board_owner *,const struct a04_board_ops *,uint32_t);
enum a04_board_result a04_board_finish_clocks(struct a04_board_owner *,const struct a04_board_ops *,uint32_t);
/* Success retains the parent lease. It is not a power or card-ready proof. */
enum a04_board_result a04_board_acquire_clocks(struct a04_board_owner *,
    const struct a04_board_ops *,uint32_t generation);
/* Refuse to release while I/O, DMA or MMIO may still be active. No clock or
 * regulator is disabled here. Failed preparation may have modified clocks;
 * it stays poisoned and the lease is kept for an external recovery owner.
 * Parent release must atomically exclude new requests and power transitions. */
enum a04_board_result a04_board_release(struct a04_board_owner *,const struct a04_board_ops *);
#endif
