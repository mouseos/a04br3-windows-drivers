/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "watchdog_policy.h"
int main(void)
{
    assert(mt8167_watchdog_should_feed(0,0));
    assert(mt8167_watchdog_should_feed(UINT64_MAX,0));
    assert(mt8167_watchdog_should_feed(5999999999ULL,600));
    assert(!mt8167_watchdog_should_feed(6000000000ULL,600));
    assert(!mt8167_watchdog_should_feed(6000000001ULL,600));
    assert(mt8167_watchdog_should_feed(4294967295ULL,600));
    assert(!mt8167_watchdog_thermal_may_feed(1,0,0));
    assert(!mt8167_watchdog_thermal_may_feed(1,5999999999ULL,600));
    assert(mt8167_watchdog_thermal_may_feed(0,5999999999ULL,600));
    assert(!mt8167_watchdog_thermal_may_feed(0,6000000000ULL,600));
    return 0;
}
