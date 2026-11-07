/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_HI556_POWER_SEQUENCE_H
#define A04_HI556_POWER_SEQUENCE_H
#include <stdint.h>
/* Caller is the referenced board power provider with I2C1 lock held.
 * Each operation is reference-counted by the real common resource owner.
 * A failed operation is uncertain: retain all attempted resources for an
 * explicit rundown; do not run a guessed inverse or reset other consumers. */
struct a04_hi556_power_ops {
 void *context;
 int (*qualified)(void *);
 int32_t (*reset)(void *,int asserted);
 int32_t (*pdn)(void *,int operating);
 int32_t (*rail)(void *,unsigned rail,unsigned microvolts);
 int32_t (*clock)(void *,unsigned hertz);
 int32_t (*wait_ms)(void *,unsigned milliseconds);
};
struct a04_hi556_power_state {unsigned attempted,ready,uncertain;int32_t raw;};
/* Source-backed order only; no MMIO provider or fake power acknowledgement. */
int a04_hi556_power_start(const struct a04_hi556_power_ops *,struct a04_hi556_power_state *);
#endif
