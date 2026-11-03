/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_WATCHDOG_POLICY_H
#define MT8167_WATCHDOG_POLICY_H
#include <stdint.h>
/* 20 ms timer, feed every 50 ticks. A nonzero deadline stops all future feeds. */
static int mt8167_watchdog_should_feed(uint32_t ticks, uint32_t deadline_seconds)
{
    if (!ticks || ticks % 50u) return 0;
    if (deadline_seconds && ticks >= deadline_seconds * 50u) return 0;
    return 1;
}
#endif
