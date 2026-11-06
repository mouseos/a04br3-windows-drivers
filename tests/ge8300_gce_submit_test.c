/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "../drivers/gpu/mt8167/gce/gce_submit.h"
struct fake {uint32_t regs[1024],off[20],val[20];unsigned writes,barriers,delays;int authorized,fail_at,stuck;};
static int auth(void *p){return ((struct fake *)p)->authorized;}
static int rd(void *p,uint32_t off,uint32_t *v){struct fake *f=p;assert(off<0x1000 && !(off&3));*v=f->regs[off/4];return 0;}
static int wr(void *p,uint32_t off,uint32_t v){struct fake *f=p;assert(f->writes<20);f->off[f->writes]=off;f->val[f->writes++]=v;if(f->fail_at && f->writes==(unsigned)f->fail_at)return -1;f->regs[off/4]=(off==0x100 && !f->stuck)?0:v;return 0;}
static void bar(void *p){((struct fake *)p)->barriers++;}
static void delay(void *p){((struct fake *)p)->delays++;}
static int restored(void *p,uint32_t l0,uint32_t l3)
{return ((struct fake *)p)->authorized && l0==0x90000000 && l3==0x91000000;}
int main(void)
{
    struct fake f={0},validation={0};_Alignas(8) uint32_t buf[1024]={0};
    struct a04_gce_submit s={&f,auth,rd,wr,bar,delay,buf,0x80001000,A04_GCE_IDLE};
    assert(a04_gce_kick_primary(&s) && !f.writes);
    assert(a04_gce_prepare_primary(&s,27,0x90000000,0x91000000) && !f.writes && s.state==A04_GCE_IDLE);
    f.authorized=1;f.regs[0x104/4]=1;
    assert(a04_gce_prepare_primary(&s,27,0x90000000,0x91000000) && !f.writes && s.state==A04_GCE_IDLE);
    f.regs[0x104/4]=0;assert(!a04_gce_prepare_primary(&s,27,0x90000000,0x91000000));
    assert(s.state==A04_GCE_PREPARED && f.writes==5 && f.barriers==1);
    assert(a04_gce_submit_irq(&s) && f.writes==5 && s.state==A04_GCE_PREPARED);
    assert(!a04_gce_kick_primary(&s));
    assert(s.state==A04_GCE_RUNNING && f.writes==7 && f.barriers==2);
    assert(f.off[0]==0x100 && f.off[1]==0x030 && f.off[2]==0x124 && f.off[3]==0x120);
    assert(f.val[2]==0x1020ac00 && f.val[3]==0x80001000 && f.off[5]==0x114 && f.off[6]==0x104);
    assert(buf[0]==0x80000000 && buf[2]==0x8001);
    assert(a04_gce_submit_irq(&s)==1 && s.state==A04_GCE_RUNNING && f.writes==7);
    f.regs[0x110/4]=1;assert(!a04_gce_submit_irq(&s) && s.state==A04_GCE_IRQ_DONE);
    assert(f.off[7]==0x110 && f.val[7]==~1u);
    f.regs[0x10c/4]=2;
    assert(!a04_gce_stop_completed(&s) && s.state==A04_GCE_STOPPED);
    assert(f.off[8]==0x108 && f.off[9]==0x100 && f.off[10]==0x114 && f.off[11]==0x104);
    assert(a04_gce_stop_completed(&s) && s.state==A04_GCE_STOPPED);
    assert(a04_gce_prepare_restore(&s,27,0x90000000,0x91000000,0,&f) && s.state==A04_GCE_STOPPED && f.writes==12);
    assert(a04_gce_prepare_restore(&s,27,0x90000000,0x92000000,restored,&f) && s.state==A04_GCE_STOPPED && f.writes==12);
    f.regs[0x110/4]=0;
    assert(a04_gce_prepare_restore(&s,27,0x90000000,0x91000000,restored,&validation) && s.state==A04_GCE_STOPPED);
    validation.authorized=1;assert(!a04_gce_prepare_restore(&s,27,0x90000000,0x91000000,restored,&validation));
    assert(s.state==A04_GCE_PREPARED && buf[4]==0x90000000 && buf[6]==0x91000000 && f.writes==17);
    memset(&f,0,sizeof(f));s.state=A04_GCE_STOPPED;f.authorized=1;f.regs[0x110/4]=1;
    assert(a04_gce_prepare_restore(&s,27,0x90000000,0x91000000,restored,&f) && s.state==A04_GCE_RETAIN && !f.writes);
    memset(&f,0,sizeof(f));s.state=A04_GCE_IDLE;f.authorized=1;
    assert(!a04_gce_prepare_primary(&s,27,0x90000000,0x91000000));
    assert(!a04_gce_kick_primary(&s));
    f.regs[0x110/4]=2;
    assert(a04_gce_submit_irq(&s)==-2 && s.state==A04_GCE_RETAIN && f.off[7]==0x110);
    memset(&f,0,sizeof(f));s.state=A04_GCE_IDLE;f.authorized=1;
    assert(!a04_gce_prepare_primary(&s,27,0x90000000,0x91000000));
    assert(!a04_gce_kick_primary(&s));
    f.regs[0x110/4]=1;f.fail_at=8;
    assert(a04_gce_submit_irq(&s)==-1 && s.state==A04_GCE_RETAIN);
    memset(&f,0,sizeof(f));s.state=A04_GCE_IRQ_DONE;f.authorized=1;f.regs[0x104/4]=1;
    assert(a04_gce_stop_completed(&s) && s.state==A04_GCE_RETAIN && f.delays==1000);
    memset(&f,0,sizeof(f));s.state=A04_GCE_IRQ_DONE;
    assert(a04_gce_stop_completed(&s) && s.state==A04_GCE_IRQ_DONE && !f.writes);
    f.authorized=1;f.fail_at=1;
    assert(a04_gce_stop_completed(&s) && s.state==A04_GCE_RETAIN);
    memset(&f,0,sizeof(f));s.state=A04_GCE_IDLE;f.authorized=1;f.stuck=1;
    assert(a04_gce_prepare_primary(&s,38,0x90000000,0x91000000) && s.state==A04_GCE_RETAIN && f.writes==1 && f.delays==1000);
    memset(&f,0,sizeof(f));s.state=A04_GCE_IDLE;f.authorized=1;f.fail_at=7;
    assert(!a04_gce_prepare_primary(&s,38,0x90000000,0x91000000));
    assert(a04_gce_kick_primary(&s) && s.state==A04_GCE_RETAIN);
    memset(&f,0,sizeof(f));s.state=A04_GCE_IDLE;f.authorized=1;
    assert(!a04_gce_prepare_primary(&s,38,0x90000000,0x91000000));
    assert(!a04_gce_kick_primary(&s));
    a04_gce_submit_timeout(&s);assert(s.state==A04_GCE_RETAIN && a04_gce_submit_irq(&s));
    return 0;
}
