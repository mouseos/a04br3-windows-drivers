/* SPDX-License-Identifier: MIT */
#ifndef A04_LC709203F_MT8167_I2C_H
#define A04_LC709203F_MT8167_I2C_H
#include <stdint.h>
#include "../../bus/mt8167-i2c/pio.h"
struct a04_gauge_mt8167 {
    const struct a04_i2c_io *io;
    int faulted;
};
/* Exclusive ownership of initialized I2C2 and a lock shared with charger
 * clients are prerequisites. A failure latches until owner-led recovery. */
int a04_gauge_mt8167_read(void *context, uint8_t address, uint8_t command,
                          uint8_t response[3]);
#endif
