/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "rogue_power.h"
struct mock { uint32_t reg[4][1024]; unsigned delays, mfg_writes; int no_status; };
static uint32_t read_reg(void *context, unsigned bank, unsigned offset)
{
    struct mock *m=context;
    assert(bank<4 && offset/4<1024 && !(offset&3));
    if (bank==A04_RGX_SPM && (offset==0x60c || offset==0x610))
        return m->no_status ? 0 : (1u<<25)|(1u<<24)|(1u<<4);
    return m->reg[bank][offset/4];
}
static void write_reg(void *context, unsigned bank, unsigned offset, uint32_t value)
{
    struct mock *m=context;
    assert(bank<4 && offset/4<1024 && !(offset&3));
    if (bank==A04_RGX_MFG) ++m->mfg_writes;
    m->reg[bank][offset/4]=value;
}
static void delay(void *context,unsigned us)
{ struct mock *m=context; assert(us==10); ++m->delays; }
int main(void)
{
    struct mock m={0};
    struct a04_rgx_io io={&m,read_reg,write_reg,delay};
    assert(a04_rgx_power_on_parent(&io,3)==6);
    assert(m.mfg_writes==0);
    m.no_status=1;
    assert(a04_rgx_power_on(&io)==1 && m.delays==10000 && m.mfg_writes==0);
    m.no_status=0; m.delays=0;
    assert(a04_rgx_power_on(&io)==0 && m.mfg_writes==1);
    assert(a04_rgx_mmpll_hz(0,0,0,0,0,0,0)==0);
    puts("GPU power sequencing mock tests passed");
    return 0;
}
