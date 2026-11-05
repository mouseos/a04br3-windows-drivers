/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "recovery.h"
struct fake { unsigned reads,writes,fail_write; uint8_t reg[8],written[2]; uint64_t now; };
static uint64_t clock_ns(void *p){return ((struct fake *)p)->now;}
static int rd(void *p,uint8_t r,uint8_t *v){struct fake *f=p;++f->reads;*v=f->reg[r];if(r==1 && f->writes==2)*v=0;return 0;}
static int wr(void *p,uint8_t r,uint8_t v){struct fake *f=p;assert(r==2 && f->writes<2);f->written[f->writes++]=v;return f->writes==f->fail_write;}
int main(void){
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
