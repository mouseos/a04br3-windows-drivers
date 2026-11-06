/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_copy_state.h"
#include "clear_image.h"
#include "render_resources.h"
#include "gpu_mmu.h"
int main(void)
{
    struct a04_vendor_wide_state w;
    struct a04_vendor_copy_state old;
    struct a04_vendor_copy_state_input in={A04_VENDOR_RENDER_SCRATCH_VA+0x10000u,
        A04_VENDOR_RENDER_SCRATCH_VA+0x11000u,A04_VENDOR_RENDER_SCRATCH_VA+0x12000u};
    _Alignas(8) struct a04_vendor_clear_image clear;
    _Alignas(8) uint32_t pc[1024]={0};
    _Alignas(8) uint64_t pd[512]={0},pt[4][512]={{0}},dest[1000],source[1000];
    struct a04_gpu_mmu_leaf leaves[4];unsigned i;
    assert(!a04_vendor_copy_state_build(&old,&in));
    assert(!a04_vendor_copy_wide_state_build(&w,in.source_va,32,32,32));
    assert(!memcmp(w.shared,old.shared,sizeof(w.shared)));
    assert(!a04_vendor_clear_image_build(&clear,A04_VENDOR_RENDER_SCRATCH_VA,0));
    assert(w.tile_size==clear.payload[6] && !memcmp(w.csr,clear.payload+32,sizeof(w.csr)));
    assert(!a04_vendor_copy_wide_state_build(&w,A04_VENDOR_WIDE_SOURCE_VA,800,1280,800));
    assert(w.tile_size==0x00320050u && w.csr[2]==1279 && w.csr[3]==799);
    assert(a04_vendor_copy_wide_state_build(&w,A04_VENDOR_WIDE_SOURCE_VA,800,1280,799));
    assert(a04_vendor_copy_wide_state_build(&w,A04_VENDOR_WIDE_SOURCE_VA+1,800,1280,800));
    assert(a04_vendor_copy_wide_state_build(&w,(UINT64_C(1)<<40)-128u,800,1280,800));
    assert(a04_vendor_copy_wide_state_build(&w,A04_VENDOR_WIDE_SOURCE_VA,16369,1,16384));
    assert(!a04_vendor_copy_wide_state_build(&w,A04_VENDOR_WIDE_SOURCE_VA,16368,1,16384));
    for(i=0;i<4;i++){leaves[i].cpu=pt[i];leaves[i].dma=0x10002000u+(uint64_t)i*4096u;}
    for(i=0;i<1000;i++){dest[i]=0x20000000u+(uint64_t)i*4096u;source[i]=0x30000000u+(uint64_t)i*4096u;}
    assert(!a04_gpu_mmu_map_pages_multi_4k(pc,pd,0x10001000,leaves,2,
        UINT64_C(0x8000000000),dest,1000,A04_GPU_MMU_SLC_BYPASS));
    assert(!a04_gpu_mmu_map_pages_multi_4k(pc,pd,0x10001000,leaves+2,2,
        A04_VENDOR_WIDE_SOURCE_VA,source,1000,A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_READ_ONLY));
    for(i=0;i<1000;i++) {
        assert(pt[i/512][i%512]==(dest[i]|A04_GPU_MMU_SLC_BYPASS|1u));
        assert(pt[2+i/512][i%512]==(source[i]|A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_READ_ONLY|1u));
    }
    assert(pd[0]==(leaves[0].dma|1u) && pd[2]==(leaves[2].dma|1u));
    assert(pt[1][488]==0 && pt[3][488]==0);
    assert(a04_gpu_mmu_map_pages_multi_4k(pc,pd,0x10001000,leaves+2,2,
        UINT64_C(0x8000000000),source,1000,A04_GPU_MMU_SLC_BYPASS));
    assert(pt[0][0]==(dest[0]|A04_GPU_MMU_SLC_BYPASS|1u));
    puts("Wide native22 geometry and disjoint read-only source mappings passed");
    return 0;
}
