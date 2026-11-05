/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "vemc_restore.h"
#include <assert.h>
#include <string.h>
struct fake { uint16_t vemc,selector,vio;unsigned fsm,addr,writes,waits,checks,revoke_at,fail_write,drop_qi,short_wait,init,dead_bus;uint64_t ticks; };
static int held(void *p,uint32_t g){struct fake *f=p;assert(g==7);return ++f->checks!=f->revoke_at;}
static int read32(void *p,unsigned off,uint32_t *v){struct fake *f=p;if(off==0x9c){*v=f->init;return 0;}assert(off==0xa4);if(f->dead_bus){*v=UINT32_MAX;return 0;}*v=f->fsm<<16;if(f->fsm==6)*v|=f->addr==0x508?f->vemc:f->addr==0x52e?f->selector:f->vio;return 0;}
static int write32(void *p,unsigned off,uint32_t v){struct fake *f=p;if(f->fail_write && ++f->writes==f->fail_write)return -1;if(!f->fail_write)f->writes++;
 if(off==0xa8){assert(v==1&&f->fsm==6);f->fsm=0;return 0;}assert(off==0xa0 && !f->fsm);f->addr=((v>>16)&0x7fff)*2u;
 assert(f->addr==0x508||f->addr==0x52e||f->addr==0x556);
 if(v>>31){assert(f->addr==0x508 && ((uint16_t)v&0x4000u));f->vemc=(uint16_t)v;}
 else {f->fsm=6;} return 0;}
static int wait_us(void *p,unsigned us){struct fake *f=p;assert(us==264);f->waits++;if(!f->short_wait)f->ticks+=264;if(!f->drop_qi)f->vemc|=0x8000;return 0;}
static uint64_t now(void *p){struct fake *f=p;return f->ticks++;}
static void init(struct fake *f){memset(f,0,sizeof(*f));f->init=1;f->vio=0xc002;f->selector=0x80;f->vemc=0x0123;}
int main(void)
{
 unsigned i,total;struct fake f;struct a04_vemc_state s={0};struct a04_vemc_result out,old;struct a04_vemc_io io={&f,held,read32,write32,wait_us,now,10000,264};
 init(&f);memset(&out,0xa5,sizeof(out));old=out;
 assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_OK && out.wrote_enable==1 && out.control==0xc123 && out.vio18==0xc002 && out.selector==0x80 && f.waits==1);
 total=f.checks;
 for(i=1;i<=total;++i){init(&f);f.revoke_at=i;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_UNOWNED && s.poisoned && !memcmp(&out,&old,sizeof(out)));}
 init(&f);f.vemc=0xc123;memset(&s,0,sizeof(s));assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_OK && !out.wrote_enable && !f.waits);
 init(&f);f.init=UINT32_MAX;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_IO && s.poisoned && !f.writes && !memcmp(&out,&old,sizeof(out)));
 init(&f);f.dead_bus=1;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_IO && s.poisoned && !f.writes && !memcmp(&out,&old,sizeof(out)));
 init(&f);f.vio=UINT16_MAX;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_IO && s.poisoned && !f.waits && !memcmp(&out,&old,sizeof(out)));
 init(&f);f.selector=UINT16_MAX;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_IO && s.poisoned && !f.waits && !memcmp(&out,&old,sizeof(out)));
 init(&f);f.vemc=UINT16_MAX;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_IO && s.poisoned && !f.waits && !memcmp(&out,&old,sizeof(out)));
 init(&f);f.vio=0x4002;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_UNSAFE && s.poisoned && !f.waits && !memcmp(&out,&old,sizeof(out)));
 init(&f);memset(&s,0,sizeof(s));assert(a04_vemc_restore(&s,&io,7,0,16,&out)==A04_VEMC_UNSAFE && s.poisoned && !f.waits);
 init(&f);f.vemc=0x8123;memset(&s,0,sizeof(s));assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_UNSAFE && s.poisoned && !f.waits);
 init(&f);f.short_wait=1;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_UNSAFE && s.poisoned && !memcmp(&out,&old,sizeof(out)));
 init(&f);f.drop_qi=1;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_MISMATCH && s.poisoned && !memcmp(&out,&old,sizeof(out)));
 init(&f);f.fail_write=7;memset(&s,0,sizeof(s));assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_IO && s.poisoned);{unsigned n=f.writes;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_POISONED && f.writes==n);}
 init(&f);f.revoke_at=3;memset(&s,0,sizeof(s));out=old;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_UNOWNED && s.poisoned && !memcmp(&out,&old,sizeof(out)));
 init(&f);memset(&s,0,sizeof(s));io.ramp_ticks=1;io.timeout_ticks=2;assert(a04_vemc_restore(&s,&io,7,1,16,&out)==A04_VEMC_TIMEOUT && s.poisoned);
 return 0;
}
