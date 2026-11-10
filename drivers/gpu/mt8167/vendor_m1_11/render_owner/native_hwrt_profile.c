/* SPDX-License-Identifier: MIT
 * Board libsrv_um.so RGXAddRenderTarget 0x124a34; native bridge 0x10f874.
 * Merge inputs at 0x14ec64: 0x3e89309c,0x3fddb3d9; divisor 0x3fbb67b2.
 * Profile bits are exact binary32 round-to-nearest/even results of the
 * observed FDIV/FMUL sequence. Integer-only code is safe for kernel use. */
#include "native_hwrt_profile.h"
#include "../../wddm-render/render_layout.h"
/* Positive normal binary32 rational n/d * 2^power; all callers are bounded
 * by render geometry. Round once, ties to even, with no kernel FP state. */
static uint32_t ratio(uint64_t n,uint64_t d,int power)
{
 int e=power;uint64_t q,r;
 while(n<d){n<<=1;--e;}
 while(n>=2*d){d<<=1;++e;}
 /* Restoring division avoids adding an ARM CRT divide-helper dependency. */
 uint64_t numerator=n<<23;q=0;r=0;
 for(unsigned bit=64;bit;){--bit;r=(r<<1)|((numerator>>bit)&1u);
  if(r>=d){r-=d;q|=UINT64_C(1)<<bit;}}
 if(r>d-r || (r==d-r && (q&1)))++q;
 if(q==(UINT64_C(1)<<24)){q>>=1;++e;}
 return ((uint32_t)(e+127)<<23)|((uint32_t)q&0x7fffffu);
}
static uint32_t divided(uint32_t bits,uint32_t dimension)
{ return ratio((bits&0x7fffffu)|0x800000u,dimension,(int)(bits>>23)-150); }
static uint32_t merge_scale(uint32_t dimension)
{ return ratio((uint64_t)dimension*16u,0xbb67b2u,23); }

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
    if(!out || !s || !l || ((uintptr_t)out&7u) || !geo_render_layout_valid(width,height,geo_render_pitch(width)) ||
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
        p[i].merge_lower_x=divided(0x3e89309cu,width);
        p[i].merge_upper_x=divided(0x3fddb3d9u,width);
        p[i].merge_scale_x=merge_scale(width);
        p[i].merge_lower_y=divided(0x3e89309cu,height);
        p[i].merge_upper_y=divided(0x3fddb3d9u,height);
        p[i].merge_scale_y=merge_scale(height);
        /* Single RT: TPCStride, TEAA, macrotile, RTC are explicitly zero. */
    }
    out[0]=p[0];out[1]=p[1];return 0;
}
