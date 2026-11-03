/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include "io_gate.h"
enum { THREADS=8, ITERATIONS=20000 };
static a04_msdc_gate gate=ATOMIC_VAR_INIT(0);
static atomic_int in_section=ATOMIC_VAR_INIT(0);
static unsigned completed;
static void *run(void *unused)
{
    unsigned i;
    (void)unused;
    for (i=0; i<ITERATIONS; ++i) {
        while (!a04_msdc_gate_acquire(&gate)) sched_yield();
        assert(atomic_fetch_add(&in_section,1)==0);
        ++completed;
        assert(atomic_fetch_sub(&in_section,1)==1);
        a04_msdc_gate_release(&gate);
    }
    return 0;
}
int main(void)
{
    pthread_t threads[THREADS];
    unsigned i;
    assert(a04_msdc_gate_acquire(&gate));
    assert(!a04_msdc_gate_acquire(&gate));
    a04_msdc_gate_release(&gate);
    for (i=0; i<THREADS; ++i) assert(!pthread_create(&threads[i],0,run,0));
    for (i=0; i<THREADS; ++i) assert(!pthread_join(threads[i],0));
    assert(completed==THREADS*ITERATIONS);
    return 0;
}
