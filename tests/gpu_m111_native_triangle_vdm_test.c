/* SPDX-License-Identifier: MIT. CPU stream encoding only; never submitted. */
#include <assert.h>
#include <string.h>
#include "native_triangle_vdm.h"
static uint32_t get(const unsigned char *p,unsigned i)
{ uint32_t v;memcpy(&v,p+i*4,4);return v; }
int main(void)
{
    struct a04_m111_triangle_vdm_input p={0x65c0201000ull,0x65c0201080ull,0x65c02010f0ull,26,0x10040,0x10000,0x10200,0x10300};
    unsigned char out[84],before[84];
    assert(!a04_m111_build_triangle_vdm(out,sizeof(out),&p));
    assert(get(out,0)==0xc65 && get(out,1)==0xc0201000u);
    assert(get(out,2)==0x4c000f84 && get(out,6)==0x20000442);
    assert(get(out,7)==0x68000100 && get(out,8)==2);
    assert(get(out,13)==0x20000842 && get(out,14)==0x1a65);
    assert(get(out,16)==0x68000600 && get(out,17)==3 && get(out,20)==0xc0000000u);
    p.draw_ppp_words=27;assert(!a04_m111_build_triangle_vdm(out,sizeof(out),&p));
    assert(get(out,14)==0x1b65);memcpy(before,out,sizeof(out));
    p.first_pds_data++;assert(a04_m111_build_triangle_vdm(out,sizeof(out),&p));assert(!memcmp(out,before,sizeof(out)));
    p.first_pds_data--;p.draw_ppp=p.prefix_ppp;assert(a04_m111_build_triangle_vdm(out,sizeof(out),&p));
    p.draw_ppp=1ull<<40;assert(a04_m111_build_triangle_vdm(out,sizeof(out),&p));
    return 0;
}
