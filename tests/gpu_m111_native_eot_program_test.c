/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_eot_program.h"
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(void)
{
    struct a04_m111_eot_program p,old;uint32_t word0,word1,control,instruction;
    assert(!a04_m111_build_eot_program(&p,UINT64_C(0xba00010000),0x12345678,0x87654321));
    assert(p.usc_bytes==16 && p.pds_data_words==8 && p.pds_code_words==2);
    assert(!p.temporary_count && p.shared_count==2);
    /* The header length includes the PCO group-alignment padding. */
    assert((p.usc[0]&15u)*2u==p.usc_bytes && (p.usc[2]&0x80u));
    assert(p.usc[3]==0xc2 && p.usc[12]==0xf2 && p.usc[15]==0xff);
    assert(u32(p.pds)==0x10000 && !u32(p.pds+4));
    instruction=u32(p.pds+32);
    /* Decode the vendor DOUTW sources independently: 64-bit data slot 1,
     * 32-bit control slot 4, then issue its USC task at data slot 0. */
    assert((instruction&15u)==1u && ((instruction>>8)&127u)==1u);
    assert(((instruction>>16)&255u)==4u && (instruction>>28)==15u);
    word0=u32(p.pds+(((instruction>>8)&127u)*8u));
    word1=u32(p.pds+(((instruction>>8)&127u)*8u)+4u);
    control=u32(p.pds+(((instruction>>16)&255u)*4u));
    assert(word0==0x12345678 && word1==0x87654321 && control==0x90000002u);
    assert(u32(p.pds+36)==0xf4000002u);
    old=p;
    assert(a04_m111_build_eot_program(&p,UINT64_C(0xba40000000),0,0));
    assert(a04_m111_build_eot_program(&p,UINT64_C(0xba00010002),0,0));
    assert(!memcmp(&old,&p,sizeof(p)));
    return 0;
}
