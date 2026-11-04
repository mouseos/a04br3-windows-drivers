/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_pds_data.h"
static uint32_t word(const uint8_t *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
int main(void)
{
    struct a04_render_owned_range ranges[]={
      {0xe000005000ULL,0x100000,4096,5},
      {0x8000001000ULL,0x101000,4096,3},
      {0x8000002000ULL,0x102000,8192,3},
      {0x8000004000ULL,0x104000,49152,3}};
    struct a04_render_pds_input p={0}; struct a04_render_pds_data out,old;
    p.shader_gpu=0xe000005040ULL;p.spill_info_gpu=0x8000001000ULL;p.scratch_info_gpu=0x8000001010ULL;
    p.spill_buffer_gpu=0x8000002000ULL;p.scratch_buffer_gpu=0x8000004000ULL;
    p.doutu_control=UINT64_C(1)<<35;p.vertex_literal=0xabc;p.descriptor_literal0=4;p.descriptor_literal1=5;
    assert(!a04_render_make_pds_data(&p,ranges,4,&out));
    assert(word(out.vertex)==0x5040 && word(out.vertex+4)==8 && word(out.vertex+8)==0xabc);
    assert(word(out.descriptor)==0x1000 && word(out.descriptor+4)==0x80);
    assert(word(out.descriptor+8)==0x1010 && word(out.descriptor+12)==0x80);
    assert(word(out.descriptor+16)==4 && word(out.descriptor+20)==5);
    assert(word(out.spill_info)==0x2000 && word(out.spill_info+4)==0x80 && word(out.spill_info+8)==4);
    assert(word(out.scratch_info)==0x4000 && word(out.scratch_info+4)==0x80 && word(out.scratch_info+8)==24);
    old=out;p.scratch_buffer_gpu=0x8000003000ULL;
    assert(a04_render_make_pds_data(&p,ranges,4,&out) && !memcmp(&out,&old,sizeof(out)));
    p.scratch_buffer_gpu=0x8000004000ULL;p.doutu_control=4;
    assert(a04_render_make_pds_data(&p,ranges,4,&out) && !memcmp(&out,&old,sizeof(out)));
    p.doutu_control=0;p.shader_gpu=0x7fff00001000ULL;
    assert(a04_render_make_pds_data(&p,ranges,4,&out) && !memcmp(&out,&old,sizeof(out)));
    return 0;
}
