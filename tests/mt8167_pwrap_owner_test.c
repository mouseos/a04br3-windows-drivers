#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include "pwrap_owner.h"
struct mock { pthread_mutex_t mutex; unsigned writes, last_cmd, prev_cmd; int fsm, in_bus, overlap; };
static int lk(void *v) { return pthread_mutex_lock(&((struct mock *)v)->mutex); }
static void un(void *v) { pthread_mutex_unlock(&((struct mock *)v)->mutex); }
static uint32_t rd(void *v, unsigned off) { struct mock *m=v; if(off==0x9c) return 1; if(m->in_bus != 1) m->overlap=1; return (uint32_t)m->fsm<<16 | 0x1234; }
static void wr(void *v,unsigned off,uint32_t x) { struct mock *m=v; if(off==0xa0){m->prev_cmd=m->last_cmd;m->last_cmd=x;} m->writes++; if (off==0xa0) m->fsm=6; if(off==0xa8) m->fsm=0; }
static void delay(void *v) { (void)v; }
struct thread_arg { struct a04_pwrap_owner *owner; struct mock *m; };
static void *run(void *v) { struct thread_arg *a=v; uint16_t value; for(int i=0;i<1000;i++) assert(a04_pwrap_owner_read_buttons(a->owner,&value)==0 && value==0x1234); return 0; }
static void *runrails(void *v) { struct thread_arg *a=v; struct a04_emmc_rails_raw r; for(int i=0;i<1000;i++) assert(a04_pwrap_owner_read_emmc_rails(a->owner,&r)==0 && r.vemc_enable==0x1234 && r.vemc_selector==0x1234 && r.vio18_enable==0x1234); return 0; }
/* Track entry/exit with the same lock so overlap is observable. */
static int locktrack(void *v) { struct mock *m=v; int r=lk(v); if(!r && ++m->in_bus!=1) m->overlap=1; return r; }
static void unlocktrack(void *v) { struct mock *m=v; --m->in_bus; un(v); }
int main(void) {
 struct mock m; struct a04_pwrap_owner o; struct thread_arg a; pthread_t t1,t2,t3; uint16_t value=0xbeef;
 memset(&m,0,sizeof m); memset(&o,0,sizeof o); assert(!pthread_mutex_init(&m.mutex,0));
 o.lock_context=&m; o.lock=locktrack; o.unlock=unlocktrack; o.bus.context=&m; o.bus.read32=rd; o.bus.write32=wr; o.bus.delay=delay; o.bus.attempts=2;
 assert(a04_pwrap_owner_read_buttons(&o,&value)==-1 && value==0xbeef && !m.writes);
 assert(!a04_pwrap_owner_start(&o)); a.owner=&o;a.m=&m;
 assert(!pthread_create(&t1,0,run,&a)); assert(!pthread_create(&t2,0,run,&a)); assert(!pthread_create(&t3,0,runrails,&a)); pthread_join(t1,0); pthread_join(t2,0); pthread_join(t3,0); assert(!m.overlap && m.writes==10000);
 { struct a04_rtc32k_raw rtc={0,0}; unsigned before=m.writes;
  assert(!a04_pwrap_owner_read_rtc32k(&o,&rtc));
  assert(rtc.pdn1==0x1234 && rtc.con==0x1234 && m.writes==before+4);
  assert(m.prev_cmd==((0x802cu>>1)<<16) && m.last_cmd==((0x803eu>>1)<<16));
 }
 assert(!a04_pwrap_owner_stop(&o)); assert(a04_pwrap_owner_read_buttons(&o,&value)==-1 && m.writes==10004);
 o.bus.poisoned=1; assert(a04_pwrap_owner_start(&o)==-1);
 pthread_mutex_destroy(&m.mutex); return 0;
}
