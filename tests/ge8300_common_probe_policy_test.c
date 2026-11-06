/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/wddm-display/shared_common_policy.h"
int main(void)
{
    struct a04_shared_common_once s={0};
    assert(a04_common_may_attempt(&s));
    assert(a04_common_claim_attempt(&s));assert(!a04_common_may_attempt(&s));
    assert(!a04_common_claim_attempt(&s));
    s.attempted=0;assert(a04_common_claim_attempt(&s));assert(!a04_common_claim_attempt(&s));
    assert(a04_display_common_range_valid(0x51000000u,0x1000u,4096000u));
    assert(!a04_display_common_range_valid(0,0x1000u,4096000u));
    assert(!a04_display_common_range_valid(0x51000001u,0x1000u,4096000u));
    assert(!a04_display_common_range_valid(0xfff00000u,0x1000u,4096000u));
    assert(!a04_display_common_range_valid(0x51000000u,0x1001u,4096000u));
    assert(!a04_display_common_range_valid(0x51000000u,0x1000u,4096u));
    return 0;
}
