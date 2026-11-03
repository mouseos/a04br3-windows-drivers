/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "watchdog_policy.h"
int main(void)
{
    assert(!mt8167_watchdog_should_feed(0,0));
    assert(!mt8167_watchdog_should_feed(49,0));
    assert(mt8167_watchdog_should_feed(50,0));
    assert(mt8167_watchdog_should_feed(29950,600));
    assert(!mt8167_watchdog_should_feed(30000,600));
    assert(!mt8167_watchdog_should_feed(30050,600));
    return 0;
}
