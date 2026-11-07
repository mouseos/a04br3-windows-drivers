/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include "native_activation_policy.h"
int main(void)
{
    assert(a04_native_activation_allowed(1,0,1,1,1,0,0,0,0));
    assert(!a04_native_activation_allowed(0,0,1,1,1,0,0,0,0));
    assert(!a04_native_activation_allowed(1,1,1,1,1,0,0,0,0));
    assert(!a04_native_activation_allowed(1,0,0,1,1,0,0,0,0));
    assert(!a04_native_activation_allowed(1,0,1,1,0,0,0,0,0));
    assert(!a04_native_activation_allowed(1,0,1,1,1,1,0,0,0));
    assert(!a04_native_activation_allowed(1,0,1,1,1,0,1,0,0));
    assert(!a04_native_activation_allowed(1,0,1,1,1,0,0,1,0));
    assert(!a04_native_activation_allowed(1,0,1,1,1,0,0,0,1));
    assert(!a04_native_activation_allowed(1,0,1,0,1,0,0,0,0));
    return 0;
}
