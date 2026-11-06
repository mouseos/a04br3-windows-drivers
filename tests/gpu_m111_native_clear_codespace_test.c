/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_clear_codespace.h"
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(void)
{
    struct a04_m111_clear_codespace p,old;uint32_t ins,control;unsigned i;
    assert(!a04_m111_build_clear_codespace(&p,UINT64_C(0xba00010000),
        UINT64_C(0xa800010000),0x11223344,0x55667788,0x99aabbcc));
    assert(u32(p.pds)==0x10000 && u32(p.pds+0x80)==0x10010);
    assert(u32(p.pds+0x20)==0xf4000002u);
    ins=u32(p.pds+0x50);
    /* Decode DOUTW and follow its constant operands, independently of layout. */
    assert((ins>>28)==15 && (ins&15)==1 && ((ins>>26)&1)==1);
    assert(!((ins>>27)&1)); /* unconditional */
    assert(u32(p.pds+0x40+8*((ins>>8)&127))==0x11223344);
    control=u32(p.pds+0x40+4*((ins>>16)&255));
    assert(control==0x90000002u);
    assert(u32(p.pds+0x88)==0x55667788 && u32(p.pds+0x8c)==0x99aabbcc);
    for(i=0x24;i<0x40;i++)assert(!p.pds[i]);
    old=p;
    assert(a04_m111_build_clear_codespace(&p,UINT64_C(0xba3ffffff0),
        UINT64_C(0xa800010000),0,0,0));
    assert(a04_m111_build_clear_codespace(&p,UINT64_C(0xba00010000),
        UINT64_C(0xa83fffff60),0,0,0));
    assert(a04_m111_build_clear_codespace(&p,UINT64_C(0xba00010000),
        UINT64_C(0xa800010008),0,0,0));
    assert(!memcmp(&p,&old,sizeof(p)));
    assert(!a04_m111_build_clear_codespace(&p,UINT64_C(0xba3fffffe0),
        UINT64_C(0xa83fffff50),0,0,0));
    return 0;
}
