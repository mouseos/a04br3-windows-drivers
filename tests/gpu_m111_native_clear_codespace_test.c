/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_clear_codespace.h"
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(void)
{
    struct a04_m111_clear_codespace p,old;
    struct a04_m111_clear_code_bindings b,before;uint32_t ins,control;unsigned i;
    assert(!a04_m111_build_clear_codespace(&p,UINT64_C(0xba00010000),
        UINT64_C(0xa800010000),0x11223344,0x55667788,0x99aabbcc));
    assert(u32(p.pds)==0x10000 && u32(p.pds+0x80)==0x10010);
    assert(u32(p.pds+0x20)==0xf4000002u);
    assert(u32(p.pds+0xc0)==0x10020 && !u32(p.pds+0xc4));
    assert(u32(p.pds+0xe0)==0xf4000002u);
    assert(u32(p.usc+0x20)==0x0580a044 && u32(p.usc+0x24)==0xff000000u);
    ins=u32(p.pds+0x50);
    /* Decode DOUTW and follow its constant operands, independently of layout. */
    assert((ins>>28)==15 && (ins&15)==1 && ((ins>>26)&1)==1);
    assert(!((ins>>27)&1)); /* unconditional */
    assert(u32(p.pds+0x40+8*((ins>>8)&127))==0x11223344);
    control=u32(p.pds+0x40+4*((ins>>16)&255));
    assert(control==0x90000002u);
    assert(u32(p.pds+0x88)==0x55667788 && u32(p.pds+0x8c)==0x99aabbcc);
    for(i=0x24;i<0x40;i++)assert(!p.pds[i]);
    assert(!a04_m111_build_clear_code_bindings(&b,UINT64_C(0xa800010000)));
    /* Decode addresses and unit counts using Rogue register field positions. */
    assert((uint32_t)b.background[0]==0x10000);
    assert((b.background[0]>>32)==0x10050);
    assert((b.background[1]>>32)==0x10040 && !(uint32_t)b.background[1]);
    assert((b.background[2]>>55)==1 && ((b.background[2]>>16)&127)==1);
    assert((b.background[2]&15)==1 && b.background_pixel_data_bytes==32);
    assert((b.pixel_event_info>>9)==1 && ((b.pixel_event_info>>5)&15)==0);
    assert((b.pixel_event_info&31)==2);
    assert(b.pixel_event_code==0x100a0 && b.pixel_event_data==0x10080);
    assert(b.ta_ppp_pds_base==0x100c0);
    before=b;
    assert(a04_m111_build_clear_code_bindings(&b,UINT64_C(0xa83fffff60)));
    assert(!memcmp(&b,&before,sizeof(b)));
    old=p;
    assert(a04_m111_build_clear_codespace(&p,UINT64_C(0xba3ffffff0),
        UINT64_C(0xa800010000),0,0,0));
    assert(a04_m111_build_clear_codespace(&p,UINT64_C(0xba00010000),
        UINT64_C(0xa83fffff60),0,0,0));
    assert(a04_m111_build_clear_codespace(&p,UINT64_C(0xba00010000),
        UINT64_C(0xa800010008),0,0,0));
    assert(!memcmp(&p,&old,sizeof(p)));
    assert(!a04_m111_build_clear_codespace(&p,UINT64_C(0xba3fffffd8),
        UINT64_C(0xa83fffff10),0,0,0));
    return 0;
}
