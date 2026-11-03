/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BU21025_H
#define A04_BU21025_H

#include <stdint.h>

/* The caller owns the bus, power and synchronization. Callbacks return zero
 * only for a fully completed transfer; delay_us must wait at least that long.
 * This interface must be called at an execution level where they may wait. */
struct a04_bu21025_io {
    void *context;
    int (*write_byte)(void *context, uint8_t value);
    int (*read_pair)(void *context, uint8_t value[2]);
    void (*delay_us)(void *context, unsigned microseconds);
};

struct a04_bu21025_sample {
    uint16_t x, y, z;
};

/* These routines do not configure clocks, I2C, GPIO, power or Windows HID.
 * Call init after the controller is powered, then serialize all sampling.
 * sample is changed only when every measurement and the final standby command
 * succeed. After a transfer failure no further command is issued; bus and
 * controller recovery belongs to the caller. Raw coordinates are not
 * per-device calibration values. */
int a04_bu21025_init(const struct a04_bu21025_io *io);
int a04_bu21025_sample(const struct a04_bu21025_io *io,
                       struct a04_bu21025_sample *sample);

#endif
