/* SPDX-License-Identifier: MIT. CPU-only relocation and input validation. */
#include <assert.h>
#include <string.h>
#include "native_triangle_texture.h"
static uint64_t get(const uint8_t *p)
{ uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v; }
int main(void)
{
    struct a04_m111_triangle_texture t,before;uint8_t rgba[4]={51,153,204,255};
    assert(!a04_m111_build_triangle_texture(&t,0x65c0200000ull,rgba));
    assert(get(t.sampler)==0x240000800fffull && !get(t.sampler+8));
    assert(get(t.image)==0x60000a61ull);
    assert((((get(t.image+8)>>16)&0x3fffffffffull)<<2)==0x65c0200000ull);
    assert(!memcmp(t.texel,rgba,4));before=t;
    assert(a04_m111_build_triangle_texture(&t,0x65c0200001ull,rgba));assert(!memcmp(&t,&before,sizeof(t)));
    assert(a04_m111_build_triangle_texture(&t,1ull<<40,rgba));
    assert(!a04_m111_build_triangle_texture(&t,(1ull<<40)-4,rgba));
    return 0;
}
