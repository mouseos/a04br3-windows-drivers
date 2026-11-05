/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC_LIFETIME_H
#define A04_MSDC_LIFETIME_H
/* A single atomic word makes admission and closing linearizable. The port's
 * device extension/common buffer lifetime is NOT extended by these bits. */
#ifdef _WIN32
typedef volatile LONG a04_lifetime;
static LONG a04_life_load(a04_lifetime *s) { return InterlockedCompareExchange(s,0,0); }
static LONG a04_life_cas(a04_lifetime *s,LONG n,LONG o) { return InterlockedCompareExchange(s,n,o); }
#else
#include <stdatomic.h>
typedef atomic_int a04_lifetime;
static int a04_life_load(a04_lifetime *s) { return atomic_load(s); }
static int a04_life_cas(a04_lifetime *s,int n,int o) { atomic_compare_exchange_strong(s,&o,n); return o; }
#endif
#define A04_LIFE_CLOSED 1
#define A04_LIFE_WORKER 2
#define A04_LIFE_DMA 4
#define A04_LIFE_POISON 8
#define A04_LIFE_REMOVED 16
#define A04_LIFE_MMIO 32
static int a04_life_enter(a04_lifetime *s)
{
    int old=a04_life_load(s);
    for (;;) {
        int next;
        if (old & (A04_LIFE_CLOSED|A04_LIFE_WORKER|A04_LIFE_POISON)) return 0;
        next=a04_life_cas(s,old|A04_LIFE_WORKER,old);
        if (next==old) return 1;
        old=next;
    }
}
static int a04_life_dma_begin(a04_lifetime *s)
{
    int old=a04_life_load(s);
    for (;;) {
        int next;
        if (!(old&A04_LIFE_WORKER) || (old&(A04_LIFE_CLOSED|A04_LIFE_DMA|A04_LIFE_POISON))) return 0;
        next=a04_life_cas(s,old|A04_LIFE_DMA,old);
        if (next==old) return 1;
        old=next;
    }
}
static int a04_life_dma_end(a04_lifetime *s,int stopped)
{
    int old=a04_life_load(s),next;
    /* Closing during a transfer revokes MMIO admission. A later apparent
     * completion could have used synthetic reads, so it cannot prove DMA
     * stopped even when the transfer routine reports success. */
    do { next=a04_life_cas(s,(stopped && !(old&A04_LIFE_CLOSED)) ? old & ~A04_LIFE_DMA : old | A04_LIFE_POISON|A04_LIFE_CLOSED,old); if(next==old) return stopped && !(old&A04_LIFE_CLOSED); old=next; } while(1);
}
/* A lease records an access which began before close. It is not a bus
 * presence guarantee and cannot prevent an asynchronous external abort. */
static int a04_life_mmio_begin(a04_lifetime *s)
{
    int old=a04_life_load(s),next;
    for (;;) {
        if (old & (A04_LIFE_CLOSED|A04_LIFE_MMIO|A04_LIFE_POISON)) return 0;
        next=a04_life_cas(s,old|A04_LIFE_MMIO,old);
        if(next==old) return 1;
        old=next;
    }
}
static void a04_life_mmio_end(a04_lifetime *s)
{
    int old=a04_life_load(s),next;
    do { next=a04_life_cas(s,old&~A04_LIFE_MMIO,old); if(next==old) return; old=next; } while(1);
}
static void a04_life_close(a04_lifetime *s,int removed)
{
    int old=a04_life_load(s),next;
    do { next=a04_life_cas(s,old|A04_LIFE_CLOSED|(removed?A04_LIFE_REMOVED:0),old); if(next==old) return; old=next; } while(1);
}
static void a04_life_leave(a04_lifetime *s)
{
    int old=a04_life_load(s),next;
    do { next=a04_life_cas(s,old&~A04_LIFE_WORKER,old); if(next==old) return; old=next; } while(1);
}
static int a04_life_releasable(a04_lifetime *s)
{
    int state=a04_life_load(s);
    return (state&A04_LIFE_CLOSED) && !(state&(A04_LIFE_WORKER|A04_LIFE_DMA|A04_LIFE_POISON|A04_LIFE_MMIO));
}
#endif
