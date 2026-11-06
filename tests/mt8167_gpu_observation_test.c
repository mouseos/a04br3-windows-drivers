/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <limits.h>
#include "../drivers/bus/mt8167-shared/gpu_lease.h"
int main(void)
{
 struct a04_gpu_observation_lease a={0};unsigned first=0,second=0;
 assert(a04_gpu_observe_acquire(&a,&first)==0 && first==1);
 assert(a04_gpu_observe_acquire(&a,&second)!=0);
 assert(a04_gpu_observe_release(&a,first+1)!=0 && a.active);
 assert(a04_gpu_observe_valid(&a,first));
 assert(a04_gpu_observe_release(&a,first)==0);
 assert(!a04_gpu_observe_valid(&a,first));
 assert(a04_gpu_observe_acquire(&a,&second)==0 && second!=first);
 assert(a04_gpu_observe_release(&a,first)!=0 && a.active);
 assert(a04_gpu_observe_release(&a,second)==0);
 a.generation=UINT_MAX;assert(a04_gpu_observe_acquire(&a,&second)==0 && second==1);
 assert(a04_gpu_observe_release(&a,second)==0);
 return 0;
}
