/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_terminate_stream.h"
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(void)
{
    struct a04_m111_terminate_stream p,old;uint64_t pointer;unsigned i;
    assert(!a04_m111_build_terminate_stream(&p,UINT64_C(0x65c0358000),(49u<<18)|79u));
    pointer=((uint64_t)(u32(p.data)&255)<<32)|u32(p.data+4);
    assert(pointer==UINT64_C(0x65c0358040));
    assert(((u32(p.data)>>8)&255)==3 && u32(p.data+8)==0xc0000000u);
    assert(u32(p.data+64)==(1u<<25));
    assert(((u32(p.data+68)>>18)&511)==49 && (u32(p.data+68)&511)==79);
    assert(!u32(p.data+72));
    for(i=12;i<64;i++)assert(!p.data[i]);
    old=p;
    assert(a04_m111_build_terminate_stream(&p,UINT64_C(0x65c0358004),0));
    assert(a04_m111_build_terminate_stream(&p,UINT64_C(0x65c0358000),1u<<9));
    assert(a04_m111_build_terminate_stream(&p,UINT64_C(1)<<40,0));
    assert(!memcmp(&old,&p,sizeof(p)));
    return 0;
}
