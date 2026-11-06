/* SPDX-License-Identifier: MIT
 * Board libsrv_um.so RGXAddRenderTarget 0x124a34; native bridge 0x10f874.
 * Merge inputs at 0x14ec64: 0x3e89309c,0x3fddb3d9; divisor 0x3fbb67b2.
 * Profile bits are exact binary32 round-to-nearest/even results of the
 * observed FDIV/FMUL sequence. Integer-only code is safe for kernel use. */
#include "native_hwrt_profile.h"
static int alias(const void *a,size_t n,const void *b,size_t m)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<n : x-y<m;
}
int a04_m111_build_single_rt_hwrt_profile(struct a04_m111_hwrt_input out[2],
    uint32_t width,uint32_t height,const struct a04_m111_target_storage *s,
    const struct a04_m111_mlist_storage *l)
{
    struct a04_m111_target_storage checked;
    struct a04_m111_mlist_storage ml;
    struct a04_m111_hwrt_input p[2]={{0}};unsigned i;
    if(!out || !s || !l || ((uintptr_t)out&7u) || width!=800u ||
       (height!=600u && height!=1280u) ||
       alias(out,2u*sizeof(*out),s,sizeof(*s)) || alias(out,2u*sizeof(*out),l,sizeof(*l)))return 1;
    if(a04_m111_layout_target_storage(width,height,s->region_gpu[0],s->region_used,
           s->tail_ptrs_gpu,s->tpc_vheap_used,&checked) ||
       a04_m111_layout_pm_mlists(0x100000u,l->gpu[0],l->used,&ml))return 1;
    if(s->region_gpu[1]!=checked.region_gpu[1] || s->vheap_gpu!=checked.vheap_gpu ||
       s->tiles_x!=checked.tiles_x || s->tiles_y!=checked.tiles_y ||
       s->region_stride!=checked.region_stride || s->region_used!=checked.region_used ||
       s->tpc_bytes!=checked.tpc_bytes || s->tpc_vheap_used!=checked.tpc_vheap_used ||
       l->gpu[1]!=ml.gpu[1] || l->stride!=ml.stride || l->used!=ml.used)return 1;
    for(i=0;i<2;i++) {
        p[i].pm_mlist=l->gpu[i];p[i].tail_ptrs=s->tail_ptrs_gpu;
        p[i].region_header=s->region_gpu[i];
        /* Despite its name, the UM bridge supplies tile count, not bytes. */
        p[i].region_header_size=s->tiles_x*s->tiles_y;
        p[i].multisample=0x88;p[i].flipped_multisample=0x88;
        p[i].ppp_screen=((height-1u)<<16)|(width-1u);
        p[i].tpc_size=s->tpc_bytes;
        p[i].te_screen=((s->tiles_y-1u)<<12)|(s->tiles_x-1u);
        p[i].te_mtile1=s->tiles_x<<18;p[i].te_mtile2=s->tiles_y<<18;
        p[i].mtile_stride=s->tiles_x*s->tiles_y;
        p[i].isp_mtile_size=(s->tiles_x<<16)|s->tiles_y;
        p[i].merge_lower_x=0x39af9a61;p[i].merge_upper_x=0x3b0de3be;
        p[i].merge_scale_x=0x46089a3e;
        p[i].merge_lower_y=height==600u?0x39ea232c:0x395b80fa;
        p[i].merge_upper_y=height==600u?0x3b3d2fa8:0x3ab15cae;
        p[i].merge_scale_y=height==600u?0x45cce75c:0x465a9062;
        /* Single RT: TPCStride, TEAA, macrotile, RTC are explicitly zero. */
    }
    out[0]=p[0];out[1]=p[1];return 0;
}
