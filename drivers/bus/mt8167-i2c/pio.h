/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_I2C_PIO_H
#define A04_MT8167_I2C_PIO_H

#include <stdint.h>

/* Register accessors must provide the ordering required by the platform.
 * The caller owns controller setup and must exclusively serialize complete
 * transfers with every other client, including firmware/PMIC users. */
struct a04_i2c_io {
    void *context;
    uint16_t (*read16)(void *context, unsigned offset);
    void (*write16)(void *context, unsigned offset, uint16_t value);
    void (*delay_us)(void *context, unsigned microseconds);
};

enum a04_i2c_result {
    A04_I2C_OK = 0, A04_I2C_INVALID = -1,
    A04_I2C_BUS_ERROR = -2, A04_I2C_TIMEOUT = -3
};

/* One PIO transfer of one or two bytes, intended for the BU21025 command
 * protocol. Initialize power, clocks, pinmux and timing beforehand. After any
 * bus error or timeout the controller state is unknown: stop issuing requests
 * until the owner has recovered the controller and bus. This function does
 * not reset shared hardware. Read output changes only on success. */
enum a04_i2c_result a04_i2c_transfer(const struct a04_i2c_io *io,
                                     uint8_t address, int read,
                                     uint8_t *buffer, unsigned length);

#endif
