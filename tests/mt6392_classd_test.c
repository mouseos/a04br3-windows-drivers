#include <assert.h>
#include <string.h>
#include "mt6392_classd.h"
struct state { int calls,fail,blocked,unsupported,stop_unsupported;uint16_t regs[256];unsigned delays[2],ndelay;uint16_t writes[20];unsigned nwrites; };
static int start(void *p){return ((struct state *)p)->blocked;}
static int quiet(void *p){return ((struct state *)p)->blocked;}
static int supports(void *p,uint16_t r,uint16_t m,uint16_t v){(void)m;(void)v;return (((struct state *)p)->unsupported && r==0x10c) || (((struct state *)p)->stop_unsupported && r==0x6a);}
static int upd(void *p,uint16_t r,uint16_t m,uint16_t v){struct state*s=p;if(++s->calls==s->fail)return -1;assert(!(r&1) && r/2<256 && s->nwrites<20);s->writes[s->nwrites++]=r;s->regs[r/2]=(s->regs[r/2]&~m)|(v&m);return 0;}
static int read16(void *p,uint16_t r,uint16_t *v){struct state*s=p;if(++s->calls==s->fail)return -1;*v=s->regs[r/2];return 0;}
static int delay(void *p,uint32_t us){struct state*s=p;if(++s->calls==s->fail)return -1;assert(s->ndelay<2);s->delays[s->ndelay++]=us;return 0;}
static struct a04_mt6392_classd codec(struct state*s){struct a04_mt6392_classd c={s,start,quiet,supports,upd,read16,delay,0,0};return c;}
int main(void){struct state s={0};struct a04_mt6392_classd c=codec(&s);int n;
 s.blocked=1;assert(a04_mt6392_classd_start_muted(&c) && !s.calls && !c.poisoned);s.blocked=0;
 s.unsupported=1;assert(a04_mt6392_classd_start_muted(&c) && !s.calls && !c.poisoned);s.unsupported=0;
 s.stop_unsupported=1;assert(a04_mt6392_classd_start_muted(&c) && !s.calls && !c.poisoned);s.stop_unsupported=0;
 s.regs[0x64/2]=0x0a00;assert(!a04_mt6392_classd_start_muted(&c) && c.active && !(s.regs[0x64/2]&0xf00));
 assert(s.delays[0]==20000 && s.delays[1]==2000 && s.regs[0x52/2]==0x3401);
 {const uint16_t expected[]={0x64,0x10c,0x60,0x68,0x64,0x56,0x52,0x52,0x64};assert(s.nwrites==9 && !memcmp(s.writes,expected,sizeof(expected)));}
 s.regs[0x64/2]|=0x0a00;assert(!a04_mt6392_classd_mute(&c) && !(s.regs[0x64/2]&0xf00) && c.active);
 s.blocked=1;n=s.calls;assert(a04_mt6392_classd_stop(&c) && s.calls==n && c.active);s.blocked=0;
 assert(!a04_mt6392_classd_stop(&c) && !c.active && s.regs[0x52/2]==0x3400);
 memset(&s,0,sizeof(s));c=codec(&s);assert(!a04_mt6392_classd_start_muted(&c));s.fail=s.calls+1;assert(a04_mt6392_classd_mute(&c) && c.poisoned && c.active);
 for(n=1;n<=13;n++){memset(&s,0,sizeof(s));c=codec(&s);s.fail=n;assert(a04_mt6392_classd_start_muted(&c));assert(c.poisoned && s.calls==n);assert(a04_mt6392_classd_start_muted(&c) && s.calls==n);}
 for(n=1;n<=5;n++){memset(&s,0,sizeof(s));c=codec(&s);assert(!a04_mt6392_classd_start_muted(&c));s.calls=0;s.fail=n;assert(a04_mt6392_classd_stop(&c));assert(c.poisoned && c.active && s.calls==n);assert(a04_mt6392_classd_stop(&c) && s.calls==n);}
 return 0;
}
