/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include "A04BR3RogueRuntimeRebase.h"
int main(void)
{
    struct a04_rgx_fw_runtime r,original;IMG_PUINT32 trace=(void *)1;
    void *main=aligned_alloc(4096,A04_RGX_MAIN_BYTES),*cfg=aligned_alloc(4096,A04_RGX_CONFIG_BYTES);
    void *newmain=aligned_alloc(4096,A04_RGX_MAIN_BYTES),*newcfg=aligned_alloc(4096,A04_RGX_CONFIG_BYTES);
    assert(main && cfg && newmain && newcfg);
    assert(a04_rgx_fw_runtime_init(&r,main,0x81000000,cfg,0x82000000,26000000)==0);
    original=r;
    assert(a04_rgx_runtime_rebase(&r,main,newcfg,&trace)!=0 && r.init==original.init && trace==(void *)1);
    assert(a04_rgx_runtime_rebase(&r,newmain,newcfg,&trace)==0);
    assert((uintptr_t)r.init-(uintptr_t)newcfg==(uintptr_t)original.init-(uintptr_t)cfg);
    assert((uintptr_t)r.kctl-(uintptr_t)newmain==(uintptr_t)original.kctl-(uintptr_t)main);
    assert((uintptr_t)trace-(uintptr_t)newmain==(uintptr_t)original.trace->sTraceBuf[0].pui32TraceBuffer-(uintptr_t)main);
    assert(r.main.physical==original.main.physical && r.config.physical==original.config.physical);
    free(main);free(cfg);free(newmain);free(newcfg);return 0;
}
