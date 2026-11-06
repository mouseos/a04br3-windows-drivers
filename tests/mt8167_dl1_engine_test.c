#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "dl1_engine.h"
struct mock { uint32_t reg[256]; int calls, fail, writes, stuck_irq; };
static int rd(void *v, uint32_t off, uint32_t *p) { struct mock *m=v; if (++m->calls==m->fail) return -1; *p=m->reg[off/4]; return 0; }
static int wr(void *v, uint32_t off, uint32_t x) { struct mock *m=v; if (++m->calls==m->fail) return -1; m->writes++; m->reg[off/4]=x; if(off==0x3a8 && !m->stuck_irq)m->reg[0x3a4/4]&=~x;return 0; }
int main(void) {
 struct mock m; struct mt8167_dl1_engine e; struct mt8167_dl1_plan p; int n, i, calls;
 assert(mt8167_dl1_plan(0x1000,1024,256,48000,&p));
 memset(&m,0,sizeof m); e.io.context=&m; e.io.read32=rd; e.io.write32=wr; e.state=MT8167_DL1_IDLE;
 m.reg[0x3a0/4]=0x101;m.reg[0x3a4/4]=3;
 assert(!mt8167_dl1_prepare_masked_irq(&e) && m.reg[0x3a0/4]==0x100 && m.reg[0x3a4/4]==2);
 calls=m.calls;
 for(i=1;i<=calls;i++){memset(&m,0,sizeof m);m.fail=i;e.state=MT8167_DL1_IDLE;assert(mt8167_dl1_prepare_masked_irq(&e)==-1 && e.state==MT8167_DL1_POISONED);}
 memset(&m,0,sizeof m);e.state=MT8167_DL1_IDLE;m.reg[0x10/4]=2;
 assert(mt8167_dl1_prepare_masked_irq(&e)==-1 && !m.writes && e.state==MT8167_DL1_POISONED);
 memset(&m,0,sizeof m);e.state=MT8167_DL1_IDLE;
 m.reg[0x14/4]=0x80000000; m.reg[0x3a0/4]=0x100;
 assert(mt8167_dl1_start(&e,&p)==0); calls=m.calls;
 assert(m.reg[0x40/4]==0x1000 && m.reg[0x48/4]==0x13ff);
 assert(m.reg[0x14/4]==0x8000000a && m.reg[0x3ac/4]==64);
 assert((m.reg[0x3a0/4]&0x1f1)==0x1a1);
 m.reg[0x3a4/4]=2; assert(mt8167_dl1_interrupt(&e,&n)==0 && n==0);
 m.reg[0x3a4/4]=3; assert(mt8167_dl1_interrupt(&e,&n)==1 && n==1 && m.reg[0x3a8/4]==1);
 assert(mt8167_dl1_stop(&e)==0 && e.state==MT8167_DL1_STOPPED);
 assert(!mt8167_dl1_restart(&e,&p,0) && e.state==MT8167_DL1_RUNNING);
 assert(!mt8167_dl1_stop(&e) && !mt8167_dl1_restart(&e,&p,1));
 assert(!mt8167_dl1_stop(&e));m.reg[0x40/4]+=4;
 assert(mt8167_dl1_restart(&e,&p,0)==-1 && e.state==MT8167_DL1_POISONED);
 for(i=1;i<=calls;i++) { memset(&m,0,sizeof m); m.fail=i; e.state=MT8167_DL1_IDLE;
   assert(mt8167_dl1_start(&e,&p)==-1 && e.state==MT8167_DL1_POISONED);
   assert(mt8167_dl1_start(&e,&p)==-1);
 }
 for (i=1;i<=8;i++) { memset(&m,0,sizeof m); e.state=MT8167_DL1_IDLE;
   assert(mt8167_dl1_start(&e,&p)==0); m.fail=m.calls+i;
   m.reg[0x3a4/4]=1;
   assert(mt8167_dl1_stop(&e)==-1 && e.state==MT8167_DL1_POISONED);
   /* No early failure may skip the independent IRQ acknowledge. */
   if(i!=5)assert(m.reg[0x3a4/4]==0);
   m.fail=0;
   assert(mt8167_dl1_stop(&e)==-1 && e.state==MT8167_DL1_POISONED);
   assert(!(m.reg[0x10/4]&2) && !(m.reg[0x3a0/4]&1) && !m.reg[0x3a4/4]);
 }
 memset(&m,0,sizeof m);e.state=MT8167_DL1_IDLE;assert(!mt8167_dl1_start(&e,&p));
 m.reg[0x3a4/4]=1;m.stuck_irq=1;assert(mt8167_dl1_stop(&e)==-1 && e.state==MT8167_DL1_POISONED);
 for (i=1;i<=3;i++) { memset(&m,0,sizeof m); e.state=MT8167_DL1_IDLE;
   assert(mt8167_dl1_start(&e,&p)==0); m.reg[0x3a4/4]=1; m.fail=m.calls+i;
   assert(mt8167_dl1_interrupt(&e,&n)==-1 && e.state==MT8167_DL1_POISONED && n==0);
 }
 memset(&m,0,sizeof m); e.state=MT8167_DL1_IDLE; p.period_frames=0;
 assert(mt8167_dl1_start(&e,&p)==-1 && m.calls==0 && e.state==MT8167_DL1_IDLE);
 assert(mt8167_dl1_plan(0x1000,1024,256,48000,&p));
 memset(&m,0,sizeof m); e.state=MT8167_DL1_IDLE; m.reg[0x10/4]=2;
 assert(mt8167_dl1_start(&e,&p)==-1 && !m.writes && e.state==MT8167_DL1_POISONED);
 return 0;
}
