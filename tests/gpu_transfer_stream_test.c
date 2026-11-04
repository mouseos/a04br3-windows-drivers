/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "transfer_stream.h"
static void put32(uint8_t *p,uint32_t x) { for(unsigned i=0;i<4;++i) p[i]=(uint8_t)(x>>(i*8)); }
static void put64(uint8_t *p,uint64_t x) { for(unsigned i=0;i<8;++i) p[i]=(uint8_t)(x>>(i*8)); }
int main(void)
{
    uint8_t stream[256]={0}; struct rogue_fwif_cmd_transfer out,initial; size_t at=8;
    for(unsigned i=0;i<4;++i,at+=8) put64(stream+at,UINT64_C(0x1234000000000000)+i);
    for(unsigned i=0;i<9;++i,at+=8) put64(stream+at,UINT64_C(0xabcd000000000000)+i);
    for(unsigned i=0;i<15;++i,at+=4) put32(stream+at,0x1000u+i);
    put32(stream,(uint32_t)at);
    memset(&out,0xa5,sizeof(out));
    if(a04_gpu_decode_transfer(stream,at,0,&out) || out.regs.pds_bgnd0_base!=UINT64_C(0x1234000000000000) ||
       out.regs.pbe_wordx_mrty[8]!=UINT64_C(0xabcd000000000008) || out.regs.isp_bgobjvals!=0x1000 ||
       out.regs.isp_rgn!=0x100e || out.regs.isp_xtp_pipe_enable || out.regs.frag_screen || out.flags) return 1;
    initial=out; put32(stream+4,1);
    if(!a04_gpu_decode_transfer(stream,at,0,&out) || memcmp(&out,&initial,sizeof(out))) return 2;
    put32(stream+4,0);
    if(!a04_gpu_decode_transfer(stream,at-1,0,&out) || !a04_gpu_decode_transfer(stream,at,4,&out)) return 3;
    put32(stream+at,0x11111111); put32(stream+at+4,0x22222222); at+=8; put32(stream,(uint32_t)at);
    if(a04_gpu_decode_transfer(stream,at,A04_TRANSFER_S7_TOP|A04_TRANSFER_MULTICORE,&out) ||
       out.regs.isp_xtp_pipe_enable!=0x11111111 || out.regs.frag_screen!=0x22222222) return 4;
    if(!a04_gpu_decode_transfer(stream,at,A04_TRANSFER_S7_TOP,&out)) return 5;
    puts("GE8300 transfer stream validation passed"); return 0;
}
