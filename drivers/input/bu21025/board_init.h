/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_TOUCH_BOARD_INIT_H
#define A04_TOUCH_BOARD_INIT_H
#include <stdint.h>

/* The caller owns I2C0 and serializes GPIO and infrastructure registers with
 * every other user. Power must already be enabled. update32 must perform an
 * atomic masked update under the shared register owner's lock. A failed setup
 * can leave partial hardware changes; recovery belongs to the owner. */
enum a04_touch_register_bank { A04_TOUCH_GPIO, A04_TOUCH_TOP, A04_TOUCH_INFRA };
struct a04_touch_board_io {
    void *context;
    int (*update32)(void *context, enum a04_touch_register_bank bank,
                    unsigned offset, uint32_t mask, uint32_t bits);
    int (*write32)(void *context, enum a04_touch_register_bank bank,
                   unsigned offset, uint32_t value);
    int (*write_i2c16)(void *context, unsigned offset, uint16_t value);
};
int a04_touch_board_init(const struct a04_touch_board_io *io);
#endif
