/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_DEFAULT_PROFILE_H
#define A04_BQ_DEFAULT_PROFILE_H
#include "charger.h"
struct a04_bq_default_io {
 void *context;
 uint64_t (*now_ns)(void *);
 int (*read)(void *,uint8_t,uint8_t *);
 int (*reset)(void *);
};
enum a04_bq_default_reason { A04_DEFAULT_NOT_ATTEMPTED, A04_DEFAULT_OK,
 A04_DEFAULT_IO, A04_DEFAULT_TIME, A04_DEFAULT_CONFIG, A04_DEFAULT_BUDGET,
 A04_DEFAULT_FAULT };
/* Snapshot proves masked default configuration at read time, NOT mode or
 * watchdog-off after observation. Reads restart watchdog on supported silicon.
 * No field asserts that a future BC1.2 result or actual charge is qualified. */
struct a04_bq_default_result {
 struct a04_bq_snapshot snapshot;
 uint64_t last_access;
 unsigned stage, reset_acked, mismatch, reason, matched;
};
/* One bounded read of all registers, preserves first fault read. No retries,
 * writes, TMR_RST, or forced D+/D- detection. USB code0 only is admissible. */
int a04_bq_read_default(const struct a04_bq_default_io *,uint64_t deadline,
                         struct a04_bq_default_result *);
/* Exact RESET write followed by the same observation. ACK alone never matches. */
int a04_bq_reset_read_default(const struct a04_bq_default_io *,uint64_t deadline,
                              struct a04_bq_default_result *);
#endif
