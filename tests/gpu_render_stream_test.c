/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "render_stream.h"
static void put32(uint8_t *data,size_t *offset,uint32_t value)
{ *offset=(*offset+3u)&~(size_t)3u; memcpy(data+*offset,&value,4); *offset+=4; }
static void put64(uint8_t *data,size_t *offset,uint64_t value)
{ *offset=(*offset+7u)&~(size_t)7u; memcpy(data+*offset,&value,8); *offset+=8; }
static void putarray(uint8_t *data,size_t *offset,size_t length,uint8_t value)
{ *offset=(*offset+7u)&~(size_t)7u; memset(data+*offset,value,length); *offset+=length; }
int main(void)
{
    uint8_t data[2048]={0}; size_t n=8; uint32_t len;
    struct rogue_fwif_cmd_geom geom, sentinel;
    struct rogue_fwif_cmd_frag frag;
    put64(data,&n,0x123456789abcdef0ULL); put64(data,&n,0x1122334455667788ULL);
    put32(data,&n,11); put32(data,&n,22); put32(data,&n,33); put32(data,&n,44);
    len=(uint32_t)n; memcpy(data,&len,4); memset(&geom,0xa5,sizeof(geom));
    assert(!a04_gpu_decode_geometry(data,n,0,&geom));
    assert(geom.regs.vdm_ctrl_stream_base==0x123456789abcdef0ULL && geom.regs.ppp_ctrl==11 && geom.regs.view_idx==44);
    sentinel=geom; assert(a04_gpu_decode_geometry(data,n-1,0,&geom)); assert(!memcmp(&sentinel,&geom,sizeof(geom)));
    assert(a04_gpu_decode_geometry(data,n,A04_RENDER_QUIRK_49927,&geom));
    put32(data,&n,1); put32(data,&n,0x11223344);
    assert(!a04_gpu_decode_geometry(data,n,A04_RENDER_QUIRK_49927,&geom));
    assert(geom.regs.tpu==0x11223344);
    memset(data,0,sizeof(data)); n=8;
    for (int i=0;i<6;i++) put64(data,&n,(uint64_t)i+100);
    putarray(data,&n,sizeof(frag.regs.pbe_word),0x12);
    put64(data,&n,0x8877665544332211ULL);
    putarray(data,&n,sizeof(frag.regs.pds_bgnd),0x23);
    putarray(data,&n,sizeof(frag.regs.pds_pr_bgnd),0x34);
    putarray(data,&n,sizeof(frag.regs.usc_clear_register),0x45);
    for (uint32_t i=0;i<10;i++) put32(data,&n,i+1);
    len=(uint32_t)n; memcpy(data,&len,4);
    assert(!a04_gpu_decode_fragment(data,n,0,&frag));
    assert(frag.regs.isp_scissor_base==100 && frag.regs.tpu_border_colour_table==0x8877665544332211ULL);
    assert(frag.regs.usc_pixel_output_ctrl==1 && frag.regs.event_pixel_pds_data==8 && frag.zls_stride==9 && frag.sls_stride==10);
    assert(a04_gpu_decode_fragment(data,n-1,0,&frag));
    return 0;
}
