/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include "lifetime.h"
static a04_lifetime state=ATOMIC_VAR_INIT(0);
static atomic_int go, entered;
static void *enter_thread(void *p)
{
    (void)p;
    while (!atomic_load(&go)) {}
    if (a04_life_enter(&state)) {
        atomic_fetch_add(&entered,1);
        if (a04_life_dma_begin(&state)) a04_life_dma_end(&state,1);
        if (a04_life_mmio_begin(&state)) a04_life_mmio_end(&state);
        a04_life_leave(&state);
    }
    return 0;
}
int main(void)
{
    pthread_t t[8]; int n,i;
    assert(a04_life_mmio_begin(&state));
    assert(!a04_life_mmio_begin(&state));
    a04_life_close(&state,1);
    assert(!a04_life_mmio_begin(&state));
    assert(!a04_life_releasable(&state));
    a04_life_mmio_end(&state);
    assert(a04_life_releasable(&state));
    assert(!a04_life_mmio_begin(&state));
    atomic_store(&state,0);
    assert(a04_life_enter(&state));
    assert(a04_life_dma_begin(&state));
    a04_life_close(&state,1);
    assert(!a04_life_releasable(&state) && !a04_life_enter(&state));
    /* Removal can turn subsequent MMIO reads into sentinels; an apparent
     * success after close cannot certify that hardware DMA stopped. */
    assert(!a04_life_dma_end(&state,1)); a04_life_leave(&state);
    assert(!a04_life_releasable(&state));
    assert((a04_life_load(&state)&(A04_LIFE_DMA|A04_LIFE_POISON))==(A04_LIFE_DMA|A04_LIFE_POISON));
    atomic_store(&state,0); assert(a04_life_enter(&state)); assert(a04_life_dma_begin(&state));
    assert(a04_life_dma_end(&state,1)); a04_life_leave(&state);a04_life_close(&state,0);
    assert(a04_life_releasable(&state));
    atomic_store(&state,0); assert(a04_life_enter(&state)); assert(a04_life_dma_begin(&state));
    assert(!a04_life_dma_end(&state,0)); a04_life_leave(&state);
    assert(!a04_life_releasable(&state) && !a04_life_enter(&state));
    assert((a04_life_load(&state)&(A04_LIFE_DMA|A04_LIFE_POISON))==(A04_LIFE_DMA|A04_LIFE_POISON));
    for (n=0;n<2000;++n) {
        atomic_store(&state,0); atomic_store(&go,0); atomic_store(&entered,0);
        for (i=0;i<8;++i) assert(!pthread_create(&t[i],0,enter_thread,0));
        atomic_store(&go,1); a04_life_close(&state,1);
        for (i=0;i<8;++i) assert(!pthread_join(t[i],0));
        if (a04_life_load(&state)&A04_LIFE_DMA)
            assert(!a04_life_releasable(&state) && (a04_life_load(&state)&A04_LIFE_POISON));
        else assert(a04_life_releasable(&state));
        assert(!a04_life_enter(&state));
        assert(!a04_life_mmio_begin(&state));
    }
    return 0;
}
