/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "thermal_lease.h"
int main(void) {
 struct a04_thermal_lease s={0};
 assert(a04_lease_claim(&s,1));s.online=1;assert(a04_lease_claim(&s,0));
 assert(!a04_lease_claim(&s,1)); assert(a04_lease_claim(&s,1)); assert(a04_lease_claim(&s,2));
 assert(a04_lease_valid(&s,1)); assert(!a04_lease_valid(&s,2)); s.clocks_ready=1;
 a04_lease_release(&s,2); assert(s.clocks_ready && a04_lease_valid(&s,1));
 a04_lease_release(&s,1); assert(!s.owner && !s.clocks_ready);
 assert(!a04_lease_claim(&s,2));a04_lease_offline(&s);assert(!a04_lease_valid(&s,2));
 s.online=1;assert(!a04_lease_valid(&s,2));assert(!a04_lease_claim(&s,1));
 puts("thermal lease exclusivity and power-cycle revocation: OK");return 0;
}
