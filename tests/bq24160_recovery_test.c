/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "recovery.h"
struct fake { unsigned reads,writes,fail_write; uint8_t reg[8],written[2]; uint64_t now; };
static uint64_t clock_ns(void *p){return ((struct fake *)p)->now;}
static int rd(void *p,uint8_t r,uint8_t *v){struct fake *f=p;++f->reads;*v=f->reg[r];if(r==1 && f->writes==2)*v=0;return 0;}
static int wr(void *p,uint8_t r,uint8_t v){struct fake *f=p;assert(r==2 && f->writes<2);f->written[f->writes++]=v;return f->writes==f->fail_write;}
struct profile {uint8_t reg[8], regs[4],values[4];unsigned writes,reads,fail_write;uint64_t now;};
static uint64_t profile_now(void *p){return ((struct profile *)p)->now;}
static int profile_read(void *p,uint8_t r,uint8_t *v){struct profile *f=p;++f->reads;*v=(uint8_t)(f->reg[r] | (r==2 ? 0x80 : 0));return 0;}
static int profile_write(void *p,uint8_t r,uint8_t v){struct profile *f=p;assert(f->writes<4);f->regs[f->writes]=r;f->values[f->writes++]=v;if(f->fail_write==f->writes)return -1;f->reg[r]=v;return 0;}
static void test_profile(void){
 struct profile f={0};struct a04_bq_recovery_proof proof={1,1,1,1,1};
 struct a04_bq_recovery_io io={&f,profile_now,profile_read,profile_write};
 struct a04_bq_recovery_result prior={0};struct a04_bq_restore_result out;
 prior.stage=12;prior.hiz_outcome=A04_BQ_HIZ_CLEAR_ACK;f.reg[7]=8;
 assert(a04_bq_restore_linux(&proof,&io,100,5,&prior,&out)&&!f.reads&&!f.writes);
 assert(!a04_bq_restore_linux(&proof,&io,100,2,&prior,&out));
 assert(f.writes==4 && f.regs[0]==7 && f.values[0]==0x28 && f.regs[1]==5 && f.values[1]==0x51 &&
        f.regs[2]==3 && f.values[2]==0xa4 && f.regs[3]==2 && f.values[3]==0x2c && out.stage==5);
 memset(&f,0,sizeof(f));f.reg[7]=8;f.fail_write=2;
 assert(a04_bq_restore_linux(&proof,&io,100,0,&prior,&out) && f.writes==2 && out.stage==2);
 memset(&f,0,sizeof(f));f.reg[7]=10;
 assert(a04_bq_restore_linux(&proof,&io,100,0,&prior,&out) && !f.writes);
}
int main(void){
 test_profile();
 struct fake f={0};struct a04_bq_recovery_proof p={1,1,1,1,1};
 struct a04_bq_recovery_io io={&f,clock_ns,rd,wr};struct a04_bq_recovery_result out;
 f.reg[0]=7;f.reg[1]=2;f.reg[2]=0x8c;f.reg[4]=0x45;f.reg[7]=0x28;
 p.battery_can_supply_system=0;assert(a04_bq_recover_batovp(&p,&io,100,&out)&&!f.reads&&!f.writes);
 p.battery_can_supply_system=1;assert(!a04_bq_recover_batovp(&p,&io,100,&out));
 assert(f.reads==9 && f.writes==2 && f.written[0]==0x0d && f.written[1]==0x0c && out.stage==12 && out.hiz_outcome==A04_BQ_HIZ_CLEAR_ACK);
 f.reads=f.writes=0;f.fail_write=1;
 assert(a04_bq_recover_batovp(&p,&io,100,&out) && f.writes==1 && out.stage==9 && out.write_attempted && out.hiz_outcome==A04_BQ_HIZ_ASSERT_UNCERTAIN);
 f.reads=f.writes=0;f.fail_write=2;
 assert(a04_bq_recover_batovp(&p,&io,100,&out) && f.writes==2 && out.stage==10 && out.hiz_outcome==A04_BQ_HIZ_CLEAR_UNCERTAIN);
 f.reads=f.writes=f.fail_write=0;f.reg[7]|=2;
 assert(a04_bq_recover_batovp(&p,&io,100,&out) && !f.writes && out.hiz_outcome==A04_BQ_HIZ_NOT_ATTEMPTED);
 return 0;
}
