/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "pinmux.h"
#include <assert.h>
#include <string.h>
struct fake { uint32_t r[3];unsigned calls,writes,reads;int owned,safe,fail_at,corrupt; };
static int held(void *v,uint32_t g) {struct fake *f=v;assert(g==1);f->calls++;return f->owned;}
static int safe(void *v,uint32_t g) {struct fake *f=v;assert(g==1);return f->safe;}
static unsigned idx(unsigned off) {assert(off==0x460 || off==0x470 || off==0x480);return (off-0x460)/0x10;}
static int read32(void *v,unsigned off,uint32_t *x) {struct fake *f=v;f->reads++;*x=f->r[idx(off)];return 0;}
static int update(void *v,unsigned off,uint32_t mask,uint32_t value) {struct fake *f=v;unsigned i=idx(off);f->writes++;assert((i<2 && mask==0x7fff && value==0x1249)||(i==2 && mask==7 && value==1));if(f->fail_at && f->writes==(unsigned)f->fail_at)return -1;f->r[i]=(f->r[i]&~mask)|(f->corrupt?0:value);return 0;}
static struct a04_pinmux_ops ops(struct fake *f) {struct a04_pinmux_ops o={f,held,safe,read32,update,0};return o;}
int main(void)
{
    struct fake f={0};struct a04_pinmux_state s={0};struct a04_pinmux_ops o=ops(&f);
    f.r[0]=f.r[1]=f.r[2]=0xffff8000u;
    assert(a04_msdc_pinmux_mode(&s,&o,1)==A04_PINMUX_UNOWNED && !f.writes);
    f.owned=1;assert(a04_msdc_pinmux_mode(&s,&o,1)==A04_PINMUX_UNSAFE && !f.writes);
    f.safe=1;assert(a04_msdc_pinmux_mode(&s,&o,1)==A04_PINMUX_OK && f.writes==3 && f.reads==3);
    assert(f.r[0]==0xffff9249u && f.r[1]==0xffff9249u && f.r[2]==0xffff8001u);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.owned=f.safe=1;f.fail_at=2;
    assert(a04_msdc_pinmux_mode(&s,&o,1)==A04_PINMUX_IO && s.poisoned && f.writes==2);
    assert(a04_msdc_pinmux_mode(&s,&o,1)==A04_PINMUX_POISONED && f.writes==2);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.owned=f.safe=1;f.corrupt=1;
    assert(a04_msdc_pinmux_mode(&s,&o,1)==A04_PINMUX_MISMATCH && s.poisoned && f.writes==1);
    return 0;
}
