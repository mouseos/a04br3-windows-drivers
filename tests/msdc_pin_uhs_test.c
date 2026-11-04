/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "pinmux.h"
#include <assert.h>
#include <string.h>
struct fake { uint32_t v[6];unsigned writes,checks,lose_at;int owned,safe,fail_at,corrupt; };
static unsigned idx(unsigned x) {switch(x){case 0xe00:return 0;case 0xe10:return 1;case 0xe20:return 2;case 0xd60:return 3;case 0xd70:return 4;case 0x920:return 5;default:assert(0);return 0;}}
static int held(void *p,uint32_t g){struct fake *f=p;assert(g==1);f->checks++;return f->owned && (!f->lose_at || f->checks<f->lose_at);}
static int safe(void *p,uint32_t g){struct fake *f=p;assert(g==1);return f->safe;}
static int rd(void *p,unsigned o,uint32_t *v){struct fake *f=p;*v=f->v[idx(o)];if(f->corrupt)*v^=1;return 0;}
static int update(void *p,unsigned o,uint32_t m,uint32_t v){struct fake *f=p;unsigned i=idx(o);assert((o==0xd60&&m==0x7700&&v==0x2300)||(o==0xd70&&m==7&&v==2));if(++f->writes==(unsigned)f->fail_at)return -1;f->v[i]=(f->v[i]&~m)|v;return 0;}
static int alias(void *p,unsigned o,uint32_t m,int set){struct fake *f=p;unsigned i=idx(o);assert((o==0xe00||o==0xe10||o==0xe20||o==0x920) && (set==0||set==1));if(++f->writes==(unsigned)f->fail_at)return -1;if(set)f->v[i]|=m;else f->v[i]&=~m;return 0;}
int main(void)
{
 struct fake f={0};struct a04_pinmux_state s={0};struct a04_pinmux_ops o={&f,held,safe,rd,update,alias};unsigned i;
 for(i=0;i<6;++i)f.v[i]=0xffff8888u;
 assert(a04_msdc_pinmux_uhs(&s,&o,1)==A04_PINMUX_UNOWNED && !f.writes);
 f.owned=1;assert(a04_msdc_pinmux_uhs(&s,&o,1)==A04_PINMUX_UNSAFE && !f.writes);
 f.safe=1;assert(a04_msdc_pinmux_uhs(&s,&o,1)==A04_PINMUX_OK && f.writes==9);
 assert((f.v[0]&0x7777)==0x1111 && (f.v[1]&0x7777)==0x1111 && (f.v[2]&0x77)==0x61);
 assert((f.v[2]&0x700)==0 && (f.v[3]&0x7700)==0x2300 && (f.v[4]&7)==2 && (f.v[5]&0x3fe)==0x3fe);
 assert((f.v[0]&~0x7777u)==(0xffff8888u&~0x7777u));
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.owned=f.safe=1;f.fail_at=3;
 assert(a04_msdc_pinmux_uhs(&s,&o,1)==A04_PINMUX_IO && s.poisoned && f.writes==3);
 assert(a04_msdc_pinmux_uhs(&s,&o,1)==A04_PINMUX_POISONED && f.writes==3);
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.owned=f.safe=1;f.lose_at=3;
 assert(a04_msdc_pinmux_uhs(&s,&o,1)==A04_PINMUX_UNOWNED && s.poisoned && f.writes==1);
 return 0;
}
