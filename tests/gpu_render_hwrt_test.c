/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_hwrt.h"
#include "render_target.h"
#include "render_freelist.h"
#include "firmware.h"
#include <stdlib.h>
int main(void)
{
    struct a04_render_hwrt_params p={0};
    struct rogue_fwif_hwrtdata_common common,old_common;
    struct rogue_fwif_hwrtdata data,old_data;
    p.width=800; p.height=1280; p.tpc_stride=128; p.tpc_size=4096; p.region_header_size=64;
    p.common_fw_address=0xc0100000; p.freelist_fw_address[0]=0xc0100040; p.freelist_fw_address[1]=0xc0100080;
    p.tpc_gpu=0x8000010000ULL; p.vheap_gpu=0x8000020000ULL; p.rtc_gpu=0x8000030000ULL;
    p.pm_mlist_gpu=0x8000040000ULL; p.macrotile_gpu=0x8000050000ULL; p.region_gpu=0x8000060000ULL;
    memset(&common,0xa5,sizeof(common)); memset(&data,0x5a,sizeof(data)); old_common=common; old_data=data;
    p.region_gpu++;
    assert(a04_gpu_init_render_hwrt(&p,&common,&data));
    assert(!memcmp(&common,&old_common,sizeof(common)) && !memcmp(&data,&old_data,sizeof(data)));
    p.region_gpu--;
    assert(!a04_gpu_init_render_hwrt(&p,&common,&data));
    assert(common.screen_pixel_max==((1279u<<16)|799u));
    assert(common.te_screen==((79u<<12)|49u) && common.mtile_stride==4000u);
    assert(common.te_mtile1==(50u<<18) && common.te_mtile2==(80u<<18));
    assert(common.isp_mtile_size==((50u<<16)|80u) && common.multi_sample_ctl==0x88);
    assert(data.hwrt_data_common_fw_addr==p.common_fw_address && data.freelists_fw_addr[1]==p.freelist_fw_address[1]);
    assert(data.rgn_header_dev_addr==p.region_gpu && data.rta_ctl.max_rts==1);
    /* 64x64 GE8300 SIPF1: Linux pvr_hwrt.c, 16x16 tiles, one macrotile. */
    p.width=64; p.height=64; p.tpc_stride=0; p.rtc_gpu=0; p.macrotile_gpu=0;
    assert(!a04_gpu_init_render_hwrt(&p,&common,&data));
    assert(common.screen_pixel_max==0x003f003fu && common.te_screen==0x00003003u);
    assert(common.mtile_stride==16u && common.te_mtile1==0x00100000u && common.te_mtile2==0x00100000u && common.isp_mtile_size==0x00040004u);
    p.width=800; p.height=1280;
    assert(!a04_gpu_init_render_hwrt(&p,&common,&data));
    assert(!common.tpc_stride && !data.rtc_dev_addr && !data.macrotile_array_dev_addr);
    p.width=8192; assert(a04_gpu_init_render_hwrt(&p,&common,&data));
    {
        uint8_t *heap=aligned_alloc(4096,A04_PVR_HEAP_SIZE);
        uint64_t pages[52]; uint32_t local_table[52],global_table[52];
        struct a04_gpu_fw_arena arena={0}; struct a04_gpu_fw_object local,global;
        struct a04_gpu_render_target target={0}, old={0}; size_t saved;
        unsigned i; assert(heap);
        for (i=0;i<52;i++) pages[i]=0x90000000u+i*4096u;
        assert(!a04_gpu_fw_arena_init(&arena,heap,A04_PVR_HEAP_SIZE,0x80000000,0x20000));
        assert(!a04_gpu_fw_alloc(&arena,sizeof(struct rogue_fwif_freelist),4096,&local));
        assert(!a04_gpu_fw_alloc(&arena,sizeof(struct rogue_fwif_freelist),4096,&global));
        assert(!a04_gpu_init_render_freelist(local_table,52,0x8000100000ULL,pages,52,1,local.cpu));
        assert(!a04_gpu_init_render_freelist(global_table,52,0x8000200000ULL,pages,52,2,global.cpu));
        saved=arena.next;
        assert(a04_gpu_prepare_render_target(&arena,&local,&global,&p,&target));
        assert(arena.next==saved && !memcmp(&target,&old,sizeof(target)));
        p.width=800;
        assert(a04_gpu_prepare_render_target(&arena,&local,&local,&p,&target));
        assert(!a04_gpu_prepare_render_target(&arena,&local,&global,&p,&target));
        assert(((struct rogue_fwif_hwrtdata *)target.data.cpu)->hwrt_data_common_fw_addr==target.common.fw_addr);
        assert(((struct rogue_fwif_hwrtdata *)target.data.cpu)->freelists_fw_addr[0]==local.fw_addr);
        assert(((struct rogue_fwif_hwrtdata *)target.data.cpu)->freelists_fw_addr[1]==global.fw_addr);
        free(heap);
    }
    return 0;
}
