/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/bus/mt8167-shared/pwrap_owner.h"
#include "../drivers/system/mt6392-rtc/calendar.h"
struct fake {
 unsigned locked,locks,unlocks,count,phase,fail,rollovers,commands,clears;
 unsigned initialized,deny_lock,stuck_clear,busy_at;
 uint16_t values[7];
};
static const uint16_t addresses[10]={0x8000,0x800a,0x800c,0x800e,0x8010,0x8012,0x8014,0x8016,0x800a,0x8000};
static int lock(void *p) {struct fake*f=p;if(f->deny_lock)return -1;assert(!f->locked);f->locked=1;f->locks++;return 0;}
static void unlock(void*p) {struct fake*f=p;assert(f->locked);f->locked=0;f->unlocks++;}
static uint32_t read32(void*p,unsigned off) {
 struct fake*f=p;unsigned index=(f->commands-1)%10;assert(f->locked);
 if(off==0x9c)return f->initialized;
 assert(off==0xa4);
 if(f->phase==1) {
  if(f->fail==f->commands)return 2u<<16;
  if(index==0 || index==9)return (6u<<16)|(f->busy_at==f->commands?0x40u:0u);
  return (6u<<16)|(index==8?(f->rollovers?(f->values[0]+1)%60:f->values[0]):f->values[index-1]);
 }
 if(f->stuck_clear && f->clears)return 6u<<16;
 return 0;
}
static void write32(void*p,unsigned off,uint32_t value) {
 struct fake*f=p;assert(f->locked);
 if(off==0xa0) {
  unsigned index=f->commands%10;
  /* Every WACS command must be a read of a fixed calendar register. */
  assert(!(value&0x80000000u));assert((value&0xffffu)==0);
  assert(((value>>16)<<1)==addresses[index]);f->commands++;f->phase=1;
 }else {
  assert(off==0xa8 && value==1);f->clears++;f->phase=0;
  if(f->commands%10==0 && f->rollovers)f->rollovers--;
 }
}
static void delay(void*p) {(void)p;}
static void init(struct fake*f,struct a04_pwrap_owner*o) {
 memset(f,0,sizeof(*f));memset(o,0,sizeof(*o));f->initialized=1;
 f->values[0]=59;f->values[1]=59;f->values[2]=23;f->values[3]=29;
 f->values[4]=0xffff;f->values[5]=2;f->values[6]=56;
 o->lock_context=f;o->lock=lock;o->unlock=unlock;
 o->bus.context=f;o->bus.read32=read32;o->bus.write32=write32;o->bus.delay=delay;o->bus.attempts=3;
 assert(a04_pwrap_owner_start(o)==0);
}
int main(void) {
 struct fake f;struct a04_pwrap_owner o;struct mt6392_rtc_raw raw,sentinel;
 struct mt6392_rtc_calendar c,cs;unsigned i,n;
 memset(&sentinel,0xa5,sizeof(sentinel));memset(&cs,0xa5,sizeof(cs));
 init(&f,&o);raw=sentinel;assert(mt6392_pwrap_read_calendar(&o,&raw)==0);
 assert(f.commands==10 && f.clears==10 && !f.locked && f.locks==f.unlocks);
 assert(mt6392_rtc_decode(&raw,&c)==0 && c.year==2024 && c.weekday==4);
 for(n=1;n<=2;n++) {
  init(&f,&o);f.rollovers=n;assert(mt6392_pwrap_read_calendar(&o,&raw)==0);
  assert(f.commands==10*(n+1) && !f.locked);
 }
 init(&f,&o);f.rollovers=3;raw=sentinel;
 assert(mt6392_pwrap_read_calendar(&o,&raw)==MT6392_RTC_UNSTABLE);
 assert(!memcmp(&raw,&sentinel,sizeof(raw)) && f.commands==30 && !o.bus.poisoned);
 for(i=1;i<=10;i++) {
  init(&f,&o);f.fail=i;raw=sentinel;
  assert(mt6392_pwrap_read_calendar(&o,&raw)==MT6392_RTC_IO_ERROR);
  assert(o.bus.poisoned && !memcmp(&raw,&sentinel,sizeof(raw)) && !f.locked);
  n=f.commands;assert(mt6392_pwrap_read_calendar(&o,&raw)==MT6392_RTC_IO_ERROR && f.commands==n);
 }
 for(i=1;i<=10;i+=9) {
  init(&f,&o);f.busy_at=i;raw=sentinel;
  assert(mt6392_pwrap_read_calendar(&o,&raw)==MT6392_RTC_UNSTABLE);
  assert(!o.bus.poisoned && !memcmp(&raw,&sentinel,sizeof(raw)) && !f.locked);
 }
 init(&f,&o);f.stuck_clear=1;raw=sentinel;
 assert(mt6392_pwrap_read_calendar(&o,&raw)==MT6392_RTC_IO_ERROR && o.bus.poisoned);
 assert(!memcmp(&raw,&sentinel,sizeof(raw)) && !f.locked);
 init(&f,&o);f.initialized=0;
 assert(mt6392_pwrap_read_calendar(&o,&raw)==MT6392_RTC_IO_ERROR && !f.commands);
 init(&f,&o);assert(a04_pwrap_owner_stop(&o)==0);raw=sentinel;
 assert(mt6392_pwrap_read_calendar(&o,&raw)==MT6392_RTC_IO_ERROR && !f.commands);
 assert(!memcmp(&raw,&sentinel,sizeof(raw)));
 init(&f,&o);f.deny_lock=1;
 assert(mt6392_pwrap_read_calendar(&o,&raw)==MT6392_RTC_IO_ERROR && !f.commands);
 assert(mt6392_pwrap_read_calendar(&o,NULL)==MT6392_RTC_INVALID);
 assert(mt6392_pwrap_read_calendar(NULL,&raw)==MT6392_RTC_IO_ERROR);
 init(&f,&o);assert(mt6392_pwrap_read_calendar(&o,&raw)==0);
 raw.month=13;c=cs;assert(mt6392_rtc_decode(&raw,&c)==MT6392_RTC_INVALID && !memcmp(&c,&cs,sizeof(c)));
 puts("MT6392 RTC: transport, bounded rollover, poisoning, admission and atomic failure PASS");return 0;
}
