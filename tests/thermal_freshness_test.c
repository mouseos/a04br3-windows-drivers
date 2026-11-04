/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include "freshness.h"
int main(void) {
 struct a04_freshness s={0}; uint32_t a[3]={3000,3000,3000};
 assert(a04_freshness_update(&s,a,100,50)); a[0]++;a[1]++;
 assert(a04_freshness_update(&s,a,101,50));a[2]++;
 assert(!a04_freshness_update(&s,a,102,50));
 a[0]++;assert(!a04_freshness_update(&s,a,150,50));
 assert(a04_freshness_update(&s,a,152,50));
 assert(a04_freshness_update(&s,a,100,50));
 a[2]=0;assert(a04_freshness_update(&s,a,153,50));assert(!s.seen && !s.changed);
 puts("per-sensor freshness evidence and stale failures: OK");
}
