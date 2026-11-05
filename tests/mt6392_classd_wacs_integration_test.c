#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "pmic_read.h"
#include "mt6392_classd.h"
struct fixture {struct a04_pmic_read bus;struct a04_pmic_spk_clock clock;uint16_t regs[256],resp;unsigned fsm,cmds,writes,delays;int ignore_alias;};
static uint32_t rd(void*p,unsigned o){struct fixture*f=p;if(o==0x9c)return 1;assert(o==0xa4);return f->fsm<<16|f->resp;}
static void wr(void*p,unsigned o,uint32_t v){struct fixture*f=p;if(o==0xa8){f->fsm=0;return;}assert(o==0xa0);f->cmds++;unsigned a=(v>>16)&0x7fffu;assert(a<256);if(v>>31){f->writes++;if(a==0x10c/2){if(!f->ignore_alias)f->regs[0x108/2]&=(uint16_t)~(v&0xe);}else if(a==0x10a/2){if(!f->ignore_alias)f->regs[0x108/2]|=v&0xe;}else f->regs[a]=v&0xffffu;f->fsm=0;}else{f->resp=f->regs[a];f->fsm=6;}}
static void tiny(void*p){(void)p;}static int verify(void*p){(void)p;return 0;}
static int supports(void*p,uint16_t r,uint16_t m,uint16_t v){(void)p;if(r==0x10c && m==0xe && (v==0 || v==0xe))return 0;return a04_pmic_classd_supported(r,m,v);}
static int update(void*p,uint16_t r,uint16_t m,uint16_t v){struct fixture*f=p;if(r==0x10c && m==0xe){if(v==0xe)return a04_pmic_spk_clock_acquire(&f->bus,&f->clock);if(!v)return a04_pmic_spk_clock_release(&f->bus,&f->clock);return -1;}return a04_pmic_classd_update(&f->bus,r,m,v);}
static int read_gain(void*p,uint16_t r,uint16_t*v){return a04_pmic_classd_read_gain(&((struct fixture*)p)->bus,r,v);}
static int delay(void*p,uint32_t us){struct fixture*f=p;assert((f->delays==0 && us==20000)||(f->delays==1 && us==2000));f->delays++;return 0;}
static void init(struct fixture*f,struct a04_mt6392_classd*c){memset(f,0,sizeof(*f));memset(c,0,sizeof(*c));f->bus.context=f;f->bus.read32=rd;f->bus.write32=wr;f->bus.delay=tiny;f->bus.attempts=2;f->regs[0x108/2]=0x800a;f->regs[0x64/2]=0x0a00;c->context=f;c->verify_start=verify;c->verify_quiescent=verify;c->supports=supports;c->update_bits=update;c->read16=read_gain;c->delay_us=delay;}
int main(void){struct fixture f;struct a04_mt6392_classd c;init(&f,&c);
 assert(!a04_mt6392_classd_start_muted(&c) && c.active && f.clock.held && f.regs[0x108/2]==0x8000 && !(f.regs[0x64/2]&0xf00) && f.delays==2);
 assert(!a04_mt6392_classd_stop(&c) && !c.active && !f.clock.held && f.regs[0x108/2]==0x800a);
 init(&f,&c);f.ignore_alias=1;assert(a04_mt6392_classd_start_muted(&c) && c.poisoned && f.bus.poisoned && f.clock.held);
 {unsigned n=f.cmds;assert(a04_mt6392_classd_stop(&c) && f.cmds==n);}
 return 0;}
