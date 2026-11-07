/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_MT8167_I2C_H
#define A04_BQ_MT8167_I2C_H
#include "../lc709203f/mt8167_i2c.h"
/* Same adapter and lock as the gauge. A failed transfer latches faulted. */
int a04_bq_mt8167_read(void *context, uint8_t reg, uint8_t *value);
int a04_bq_mt8167_kick(void *context, uint8_t value);
/* Explicit reset changes every charger setting; caller owns quiesced bus. */
int a04_bq_mt8167_reset_default(void *context);
/* Recovery requires independent ownership and power-source proof. */
int a04_bq_mt8167_recovery_write(void *context,uint8_t reg,uint8_t value);
#endif
