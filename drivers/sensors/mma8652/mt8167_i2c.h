/* SPDX-License-Identifier: MIT */
#ifndef A04_MMA_MT8167_I2C_H
#define A04_MMA_MT8167_I2C_H
#include "../../bus/mt8167-i2c/pio.h"
#include <stdint.h>
struct a04_mma_i2c { const struct a04_i2c_io *io; uint64_t (*now_ns)(void *); int faulted; };
/* The caller has exclusive ownership of I2C1, has initialized it and holds
 * its lock throughout each transfer. Errors latch until ownership is reset. */
int a04_mma_i2c_read(void *, uint8_t, uint8_t, uint8_t *, unsigned);
int a04_mma_i2c_write(void *, uint8_t, uint8_t, uint8_t);
#endif
