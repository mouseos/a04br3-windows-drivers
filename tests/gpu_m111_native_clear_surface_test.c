/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_clear_surface.h"
#include "native_clear_codespace.h"
int main(void)
{
    struct a04_m111_clear_surface p,old;
    struct a04_m111_clear_codespace code;
    unsigned heights[2]={600,1280},i;
    for(i=0;i<2;i++){
        assert(!a04_m111_build_clear_surface(&p,UINT64_C(0x8000000000),800,heights[i],800));
        /* Decode native fields. Non-square clip distinguishes X/Y ordering. */
        assert((((uint64_t)(p.emit[1]&63)<<32)|p.emit[0])*4==UINT64_C(0x8000000000));
        assert(p.emit[1]==0x80020 && !p.emit[0]);
        assert((p.csr[0]>>56)==2);
        assert((((p.csr[0]>>33)&0x1fffe)+2)==800);
        assert((p.csr[0]&UINT64_C(0xffffffff))==0x68800000);
        assert((p.csr[1]>>32)==799 && (uint32_t)p.csr[1]==heights[i]-1);
        assert(!a04_m111_build_clear_codespace(&code,UINT64_C(0xba00010000),
            UINT64_C(0xa800010000),0x11223344,p.emit[0],p.emit[1]));
    }
    old=p;
    assert(a04_m111_build_clear_surface(&p,UINT64_C(0xfffffff000),800,1280,800));
    assert(a04_m111_build_clear_surface(&p,UINT64_C(0x8000000004),800,600,800));
    assert(a04_m111_build_clear_surface(&p,UINT64_C(0x8000000000),800,600,799));
    assert(!memcmp(&p,&old,sizeof(p)));
    return 0;
}
