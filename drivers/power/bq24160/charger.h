/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ24160_CHARGER_H
#define A04_BQ24160_CHARGER_H
#include <stdint.h>

/* Called only under the single I2C2 owner's lock, with an already armed
 * maintenance worker. Return zero only for a completely successful transfer. */
struct a04_bq_bus {
    void *context;
    uint64_t (*now_ns)(void *);
    int (*read)(void *, uint8_t reg, uint8_t *value);
    int (*write_watchdog)(void *, uint8_t value);
};
struct a04_bq_snapshot { uint8_t raw[8]; unsigned valid; uint64_t at; };
struct a04_bq_owner {
    struct a04_bq_snapshot first, latest;
    uint64_t last_kick;
    unsigned attempts, kicks, fault_history;
    int stopped;
};
enum a04_bq_result {
    A04_BQ_OK, A04_BQ_INVALID, A04_BQ_STOPPED, A04_BQ_DEADLINE,
    A04_BQ_READ_ERROR, A04_BQ_POLICY, A04_BQ_FAULT, A04_BQ_WRITE_ERROR
};
/* Caller holds the same lock used by LC709203F for the entire cycle. The
 * worker must already be armed and a valid firmware handoff established.
 * Failure can leave an active watchdog and eventual hardware defaults. */
enum a04_bq_result a04_bq_step(struct a04_bq_owner *, const struct a04_bq_bus *, uint64_t deadline_ns);
#endif
