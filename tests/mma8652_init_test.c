/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "mma8652.h"
struct fake { uint8_t regs[256]; int count, fail_at; };
static int rd(void *p,uint8_t addr,uint8_t reg,uint8_t *out,unsigned len)
{
    struct fake *f=p; assert(addr==0x1d && len==1);
    if (++f->count==f->fail_at) return -1;
    *out=f->regs[reg]; return 0;
}
static int wr(void *p,uint8_t addr,uint8_t reg,uint8_t value)
{
    struct fake *f=p; assert(addr==0x1d);
    if (++f->count==f->fail_at) return -1;
    if (reg==0x0e) assert(!(f->regs[0x2a]&1));
    f->regs[reg]=value; return 0;
}
int main(void)
{
    struct fake f={{0},0,0}; struct a04_mma8652_bus bus={&f,rd,wr}; int fail;
    f.regs[0x0d]=0x4a; f.regs[0x2a]=0xc7; f.regs[0x0e]=0x13;
    assert(a04_mma8652_start(&bus)==A04_MMA_OK);
    assert(f.regs[0x2a]==0xe5 && f.regs[0x0e]==0);
    for (fail=1;fail<=8;++fail) {
        struct fake bad={{0},0,0}; struct a04_mma8652_bus b={&bad,rd,wr};
        bad.regs[0x0d]=0x4a; bad.fail_at=fail;
        assert(a04_mma8652_start(&b)!=A04_MMA_OK);
    }
    puts("MMA8652 initialization tests passed"); return 0;
}
