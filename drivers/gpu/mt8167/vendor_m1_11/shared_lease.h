/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_LEASE_H
#define A04_SHARED_LEASE_H
#include <stdint.h>
/* Caller holds the GPU lifetime lock. Tokens are single-use within one
 * provider instance; exhausting the counter closes acquisition. */
struct a04_shared_lease_state { uint32_t next,active,task,flush; };
static void a04_shared_lease_copy_complete(struct a04_shared_lease_state *s,uint32_t task,uint32_t flush)
{ if(s && task && flush){s->task=task;s->flush=flush;} }
static uint32_t a04_shared_lease_acquire(struct a04_shared_lease_state *s)
{
    if(!s || s->active || !s->task || !s->flush || s->next==UINT32_MAX)return 0;
    s->active=++s->next;return s->active;
}
static int a04_shared_lease_release(struct a04_shared_lease_state *s,uint32_t token)
{
    if(!s || !token || s->active!=token)return 0;
    s->active=0;return 1;
}
#endif
