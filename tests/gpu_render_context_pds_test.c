/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_context_pds.h"
static uint64_t get64(const uint8_t *p)
{ uint64_t v=0;unsigned i;for(i=0;i<8;++i)v|=(uint64_t)p[i]<<(8u*i);return v; }
int main(void)
{
    struct a04_render_owned_range r={UINT64_C(0xda00001000),0x100000,4096,3};
    struct a04_render_context_pt_data data,old;uint64_t a=r.gpu_va+0x100;
    struct a04_render_context_pds_code code;
    assert(a04_render_make_context_pds_code(0));assert(!a04_render_make_context_pds_code(&code));
    assert(get64(code.pt_store)==UINT64_C(0xd0800001d0800000));
    assert(get64(code.pt_resume)==UINT64_C(0xd0000000d3800010));
    assert(get64(code.sr)==UINT64_C(0xf0060201f0020001));
    assert(!get64(code.pt_store+24) && !get64(code.pt_resume+24) && !code.sr[12] && !code.sr[15]);
    assert(!a04_render_make_context_pt_data(a,&r,1,&data));
    assert(get64(data.store)==(a|(UINT64_C(15)<<40)|(UINT64_C(32)<<46)));
    assert(get64(data.store+16)==((a+120)|(UINT64_C(2)<<40)|(UINT64_C(62)<<46)));
    assert(get64(data.resume)==(a|(UINT64_C(6)<<41)|(UINT64_C(16)<<47)));
    assert(get64(data.resume+16)==((a+96)|(UINT64_C(4)<<41)|(UINT64_C(28)<<47)));
    assert(!get64(data.store+24) && !get64(data.resume+24));old=data;
    assert(a04_render_make_context_pt_data(a+1,&r,1,&data) && !memcmp(&data,&old,sizeof(data)));
    assert(a04_render_make_context_pt_data(r.gpu_va+4096-64,&r,1,&data) && !memcmp(&data,&old,sizeof(data)));
    r.access=1;assert(a04_render_make_context_pt_data(a,&r,1,&data) && !memcmp(&data,&old,sizeof(data)));
    { struct a04_render_owned_range ranges[]={{UINT64_C(0xda00010000),0x200000,20480,3},
                                              {UINT64_C(0xe000001000),0x205000,4096,5}};
      uint8_t sr[64],saved[64];uint64_t state=ranges[0].gpu_va,shader=ranges[1].gpu_va+64;
      assert(!a04_render_make_context_sr_data(state,shader,UINT64_C(1)<<35,ranges,2,sr));
      assert(get64(sr)==((state&UINT64_C(0xffffffff))<<32 | (state>>32)));
      assert(get64(sr+8)==2 && get64(sr+24)==UINT64_C(0x80004002));
      assert(get64(sr+32)==((shader&UINT64_C(0xfffffffc))|(UINT64_C(1)<<35)));
      assert(!get64(sr+40) && !get64(sr+48) && !get64(sr+56));memcpy(saved,sr,sizeof(sr));
      assert(a04_render_make_context_sr_data(state,shader,UINT64_C(1)<<33,ranges,2,sr) && !memcmp(saved,sr,sizeof(sr)));
      ranges[0].size=16384;assert(a04_render_make_context_sr_data(state,shader,0,ranges,2,sr) && !memcmp(saved,sr,sizeof(sr))); }
    return 0;
}
