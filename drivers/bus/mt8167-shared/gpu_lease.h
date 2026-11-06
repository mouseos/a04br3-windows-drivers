/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_GPU_LEASE_H
#define A04_SHARED_GPU_LEASE_H
/* Protected by the parent resource mutex; no assertion of SoC ownership. */
struct a04_gpu_observation_lease { unsigned generation; int active; };
static inline int a04_gpu_observe_acquire(struct a04_gpu_observation_lease *s,unsigned *g)
{ if(!s||!g||s->active)return -1; if(++s->generation==0)++s->generation;s->active=1;*g=s->generation;return 0; }
static inline int a04_gpu_observe_valid(const struct a04_gpu_observation_lease *s,unsigned g)
{ return s && s->active && g && s->generation==g; }
static inline int a04_gpu_observe_release(struct a04_gpu_observation_lease *s,unsigned g)
{ if(!a04_gpu_observe_valid(s,g))return -1;s->active=0;return 0; }
#endif
