/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_PLATFORM_CLOCK_H
#define A04_BQ_PLATFORM_CLOCK_H
#include <stdint.h>
/* An apparently matching QPC is only a candidate: same frequency and values
 * cannot prove the same physical counter, boot epoch, ownership or D0-off
 * continuity. No result from this diagnostic grants BQ access. */
enum a04_bq_clock_result { A04_CLOCK_INVALID, A04_CLOCK_STOPPED, A04_CLOCK_INCONSISTENT, A04_CLOCK_CANDIDATE };
enum a04_bq_clock_result a04_bq_clock_compare(const uint8_t *,unsigned,uint64_t monotonic_before_ns,
                                               uint64_t before,uint64_t after,uint64_t frequency_before,uint64_t frequency_after);
#endif
