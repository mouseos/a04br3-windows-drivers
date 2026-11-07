/* SPDX-License-Identifier: MIT
 * Board GLES OutputTerminateState 0x1c1fac. FUN_001b7cb8 returns count0
 * when streamout object is null AND context+0x7e30 bit5 is clear.
 * That exact leaf emits PPP header/clip/RT0 and a VDM PPP pointer/terminate.
 * Gap zeroing is host packing policy. No vertex, USC or PDS allocation here.
 */
#include "native_terminate_stream.h"
static void le32(uint8_t *p,uint32_t v)
{ unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(v>>(8u*i)); }
int a04_m111_build_terminate_stream(struct a04_m111_terminate_stream *out,
    uint64_t page_gpu,uint32_t screen_xy_max)
{
    struct a04_m111_terminate_stream made={{0}};uint64_t ppp;
    if(!out || page_gpu<4096u || page_gpu>=(UINT64_C(1)<<40) ||
       (page_gpu&4095u) || (screen_xy_max&~0x07fc01ffu))return 1;
    ppp=page_gpu+A04_M111_TERMINATE_PPP_OFFSET;
    le32(made.data,(uint32_t)(ppp>>32)|0x300u);
    le32(made.data+4,(uint32_t)ppp);le32(made.data+8,0xc0000000u);
    le32(made.data+64,0x02000000u);le32(made.data+68,screen_xy_max);
    /* PPP third word=RT0,left0, observed in native producer. */
    *out=made;return 0;
}

/* RGXAddRenderTarget 0x124a34: dataset+0x34=ceil(width/16)-1,
 * dataset+0x38=ceil(height/16)-1. Getter 0x1258b8 returns these unchanged;
 * GLES StartFrame 0x15e9c8 packs (xmax<<18)|ymax into surface+0x150. */
int a04_m111_build_clear_terminate_stream(struct a04_m111_terminate_stream *out,
    uint64_t page_gpu,uint32_t width,uint32_t height)
{
    uint32_t x,y;
    if(!width || !height || width>4096u || height>4096u)return 1;
    x=(width+15u)/16u-1u;y=(height+15u)/16u-1u;
    return a04_m111_build_terminate_stream(out,page_gpu,(x<<18)|y);
}
