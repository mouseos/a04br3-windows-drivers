/* SPDX-License-Identifier: MIT */
#ifndef A04_BQHR_DIAGNOSTIC_H
#define A04_BQHR_DIAGNOSTIC_H
#include <stdint.h>
/* Only classifies; never adopts a record or accesses the charger. */
enum a04_bqhr_result { A04_BQHR_INVALID=0, A04_BQHR_STOPPED=1, A04_BQHR_OTHER_STATE=2 };
struct a04_bqhr_info { uint64_t state, kicks, faults, first_valid, latest_valid, reason; };
enum a04_bqhr_result a04_bqhr_classify(const uint8_t *, unsigned, struct a04_bqhr_info *);
#endif
