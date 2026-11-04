/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_WATCHDOG_POLICY_H
#define MT8167_WATCHDOG_POLICY_H
#include <stdint.h>
/* Interrupt time is monotonic, in 100 ns units; zero disables the deadline. */
static int mt8167_watchdog_should_feed(uint64_t elapsed, uint32_t deadline_seconds)
{
    return !deadline_seconds || elapsed < (uint64_t)deadline_seconds * 10000000u;
}
static int mt8167_watchdog_thermal_may_feed(int emergency, uint64_t elapsed, uint32_t deadline_seconds)
{
    return !emergency && mt8167_watchdog_should_feed(elapsed, deadline_seconds);
}
#endif
