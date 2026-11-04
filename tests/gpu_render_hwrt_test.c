/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_hwrt.h"
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
    p.width=8192; assert(a04_gpu_init_render_hwrt(&p,&common,&data));
    return 0;
}
