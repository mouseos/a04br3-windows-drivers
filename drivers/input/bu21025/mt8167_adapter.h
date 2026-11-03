/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BU21025_MT8167_ADAPTER_H
#define A04_BU21025_MT8167_ADAPTER_H

#include "bu21025.h"
#include "../../bus/mt8167-i2c/pio.h"

/* A single serialized owner is required. Once faulted, do not clear the flag
 * or create a new adapter until the controller owner has recovered hardware. */
struct a04_bu21025_mt8167 {
    const struct a04_i2c_io *bus;
    uint8_t address;
    int faulted;
};

/* Binds callbacks only; performs no hardware access. The caller must retain
 * adapter and bus for at least as long as the resulting io object. */
int a04_bu21025_mt8167_bind(struct a04_bu21025_mt8167 *adapter,
                            struct a04_bu21025_io *io);

#endif
