/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINPE_LEASE_H
#define A04_WINPE_LEASE_H
#include <stdint.h>
#ifndef A04_TOPRGU_WINPE_LEASE_SECONDS
#define A04_TOPRGU_WINPE_LEASE_SECONDS 0
#endif
#if A04_TOPRGU_WINPE_LEASE_SECONDS != 0 && A04_TOPRGU_WINPE_LEASE_SECONDS != 1800
#error WinPE lease is disabled or the fixed 1800 second development limit
#endif
#if A04_TOPRGU_WINPE_LEASE_SECONDS && !A04_TOPRGU_CPU1_HANDOFF
#error WinPE lease requires CPU1 ownership handoff
#endif
#define A04_WINPE_LEASE_MARKER "A04RGU_WINPE_LEASE_V1:1800"
enum { A04_WINPE_LEASE_CONTINUE=0, A04_WINPE_LEASE_RESET=1,
       A04_WINPE_LEASE_HOLD=2 };
struct a04_winpe_lease { uint64_t started; unsigned armed, expired; };
static inline void a04_winpe_lease_start(struct a04_winpe_lease *s,uint64_t now)
{
    if (!s->armed) { s->started=now; s->armed=1; }
}
static inline int a04_winpe_lease_poll(struct a04_winpe_lease *s,uint64_t now)
{
#if A04_TOPRGU_WINPE_LEASE_SECONDS
    if (s->expired) return A04_WINPE_LEASE_HOLD;
    if (s->armed && now>=s->started &&
        now-s->started >= (uint64_t)A04_TOPRGU_WINPE_LEASE_SECONDS*10000000u) {
        s->expired=1;
        return A04_WINPE_LEASE_RESET;
    }
#else
    (void)s;(void)now;
#endif
    return A04_WINPE_LEASE_CONTINUE;
}
#endif
