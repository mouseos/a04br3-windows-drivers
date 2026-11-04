/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MMA8652_H
#define A04_MMA8652_H
#include <stdint.h>

/* read_regs must perform one atomic register-address write followed by a
 * repeated-start read, serialized with every client of the controller. */
struct a04_mma8652_bus {
    void *context;
    int (*read_regs)(void *context, uint8_t address, uint8_t reg,
                     uint8_t *data, unsigned length);
    int (*write_reg)(void *context, uint8_t address, uint8_t reg, uint8_t value);
};
struct a04_mma8652_sample { int16_t x, y, z; };
enum a04_mma8652_result {
    A04_MMA_OK = 0, A04_MMA_IO = -1, A04_MMA_ID = -2,
    A04_MMA_INVALID = -3, A04_MMA_NOT_READY = -4
};
/* Read-only: the caller owns power, clock, configuration and bus locking.
 * Probe on each power-up and after recovery; reject all other chip IDs. */
int a04_mma8652_probe(const struct a04_mma8652_bus *bus);
/* Probe, enter standby, configure 2g / 12-bit / 50 Hz and activate.
 * On any failure the caller must stop using the device until recovery. */
int a04_mma8652_start(const struct a04_mma8652_bus *bus);
/* Requires active mode, full-resolution mode and 2g range already configured.
 * Returns signed 12-bit counts in board coordinates, 1024 counts per g.
 * Does not modify out on error. A sample is never fabricated if not ready. */
int a04_mma8652_read(const struct a04_mma8652_bus *bus,
                     struct a04_mma8652_sample *out);
#endif
