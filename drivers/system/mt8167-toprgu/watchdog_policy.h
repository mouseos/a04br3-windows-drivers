/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_WATCHDOG_POLICY_H
#define MT8167_WATCHDOG_POLICY_H
#include <stdint.h>
/* Interrupt time is monotonic, in 100 ns units; zero disables the deadline. */
static int mt8167_watchdog_should_feed(uint64_t elapsed, uint32_t deadline_seconds)
{
    return !deadline_seconds || elapsed < (uint64_t)deadline_seconds * 10000000u;
}
/* 32-bit monotonic milliseconds, modular subtraction; timeout < 2^31. */
static inline int mt8167_thermal_deadline_expired(uint32_t now,uint32_t last,uint32_t timeout)
{ return timeout && timeout<0x80000000u && (uint32_t)(now-last)>timeout; }
static inline int mt8167_thermal_must_stop(int armed,int four,uint32_t now,uint32_t last,uint32_t timeout)
{ return armed && (!four || mt8167_thermal_deadline_expired(now,last,timeout)); }
static int mt8167_watchdog_thermal_may_feed(int emergency, uint64_t elapsed, uint32_t deadline_seconds)
{
    return !emergency && mt8167_watchdog_should_feed(elapsed, deadline_seconds);
}
#endif
