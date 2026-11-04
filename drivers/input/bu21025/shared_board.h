/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_TOUCH_SHARED_BOARD_H
#define A04_TOUCH_SHARED_BOARD_H
#include "board_init.h"
#include "../../bus/mt8167-shared/register_owner.h"
/* Caller holds parent interface/lifetime reference until all touch work has
 * drained. I2C0 remains an exclusively assigned touch resource. */
struct a04_touch_shared_board {
    struct a04_register_owner *owner;
    void *i2c_context;
    int (*i2c_write16)(void *,unsigned,uint16_t);
};
int a04_touch_shared_bind(struct a04_touch_shared_board *,struct a04_touch_board_io *);
int a04_touch_shared_pen(struct a04_touch_shared_board *,int *down);
#endif
