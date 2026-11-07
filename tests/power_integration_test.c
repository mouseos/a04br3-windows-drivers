/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <string.h>
#include "cold_profile.h"
#include "boot_record.h"
#include "../drivers/bus/mt8167-shared/thermal_owner.h"
struct fixture { uint8_t raw[8];unsigned reads,writes,fail_write;uint64_t now; };
static uint64_t clock_ns(void *p){return ((struct fixture *)p)->now++;}
static int read_reg(void *p,uint8_t reg,uint8_t *v){struct fixture *f=p;assert(reg<8);f->reads++;*v=f->raw[reg];return 0;}
static int write_reg(void *p,uint8_t reg,uint8_t v){struct fixture *f=p;f->writes++;if(f->fail_write==f->writes)return -1;assert(reg==7||reg==5||reg==3||reg==2);assert(reg!=2 || v==0x0c);f->raw[reg]=v;return 0;}
static void setup(struct fixture *f,struct a04_bq_owner *o){memset(f,0,sizeof(*f));memset(o,0,sizeof(*o));f->now=100;f->raw[0]=0x30;f->raw[2]=0x8c;f->raw[4]=0x45;f->raw[7]=8;assert(!a04_bq_prepare_cold_diagnostic(o,100,1000000000));}
int main(void){
 struct fixture f;struct a04_bq_owner o;struct a04_bq_cold_profile_result r;
 struct a04_bq_recovery_io io={&f,clock_ns,read_reg,write_reg};
 setup(&f,&o);assert(!a04_bq_cold_profile(&o,&io,&r)&&r.stage==17&&f.writes==4&&o.handoff_pending==2&&!o.kicks);
 { unsigned reads=f.reads,writes=f.writes;
   assert(o.cold_profile_attempted && a04_bq_cold_profile(&o,&io,&r));
   assert(f.reads==reads && f.writes==writes && !o.stopped); }
 setup(&f,&o);f.raw[1]=2;assert(a04_bq_cold_profile(&o,&io,&r)&&!f.writes&&o.stopped); /* BATOVP: no HiZ or restore */
 setup(&f,&o);f.fail_write=2;assert(a04_bq_cold_profile(&o,&io,&r)&&f.writes==2&&r.stage==10&&o.stopped&&o.latest.valid==255);
 setup(&f,&o);o.handoff_pending=0;assert(a04_bq_cold_profile(&o,&io,&r)&&!f.reads&&!f.writes);
 setup(&f,&o);f.now=o.handoff_deadline;assert(a04_bq_cold_profile(&o,&io,&r)&&!f.reads&&!f.writes&&o.stopped);
 setup(&f,&o);f.raw[0]=0x37;assert(a04_bq_cold_profile(&o,&io,&r)&&!f.writes&&o.fault_history==(1u<<7));
 struct a04_thermal_boot_cal v={1,(260u<<17)|(260u<<8)|(40u<<1)|1,(512u<<22)|(512u<<12)|4,260u<<23};
 struct a04_thermal_cal cal,old;memset(&old,0xa5,sizeof(old));cal=old;
 assert(!a04_thermal_boot_calibration(&cal,&v,sizeof(v),6));assert(cal.gain==10000);
 cal=old;assert(a04_thermal_boot_calibration(&cal,&v,sizeof(v),0x16)&&!memcmp(&cal,&old,sizeof(cal)));
 assert(a04_thermal_boot_calibration(&cal,&v,sizeof(v)-1,6)&&!memcmp(&cal,&old,sizeof(cal)));
 v.version=2;assert(a04_thermal_boot_calibration(&cal,&v,sizeof(v),6)&&!memcmp(&cal,&old,sizeof(cal)));
 struct a04_shared_thermal_owner owner={0};uint32_t token=0;
 owner.online=1;assert(a04_shared_thermal_acquire(&owner,&token));
 assert(!a04_shared_thermal_native_acquire(&owner,&token));
 assert(!owner.firmware_released&&!owner.adc_exclusive&&owner.native_claim);
 assert(a04_shared_thermal_native_acquire(&owner,&token));
 owner.clocks_ready=1;assert(a04_shared_thermal_release(&owner,owner.generation));
 assert(!a04_shared_thermal_quiescent(&owner,owner.generation));
 assert(!a04_shared_thermal_release(&owner,owner.generation)&&!owner.native_claim);
 owner.poisoned=1;assert(a04_shared_thermal_native_acquire(&owner,&token));
 return 0;
}
