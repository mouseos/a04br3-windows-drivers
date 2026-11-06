#include <assert.h>
#include <string.h>
#include "vcdt_pwrap.h"

typedef struct { int locked, lease, denied, fail_release, timeout, commands, releases, unlocks, read_pending; } Mock;
static int lock(void *p){Mock*m=p;assert(!m->locked);m->locked=1;return 0;}
static void unlock(void *p){Mock*m=p;assert(m->locked);m->locked=0;m->unlocks++;}
static int claim(void *p){Mock*m=p;assert(m->locked && !m->lease);if(m->denied)return -1;m->lease=1;return 0;}
static int release(void *p){Mock*m=p;assert(m->locked && m->lease);m->releases++;if(m->fail_release)return -1;m->lease=0;return 0;}
static void delay_us(void *p,unsigned us){Mock*m=p;assert(m->locked && m->lease && us==100);}
static void delay(void *p){Mock*m=p;assert(m->locked);}
static uint32_t read32(void *p,unsigned reg){Mock*m=p;assert(m->locked);if(reg==0x9c)return 1;assert(reg==0xa4);if(m->timeout && m->commands)return 1u<<16;return m->read_pending?(6u<<16)|0x8000u|1200u:0;}
static void write32(void *p,unsigned reg,uint32_t val){Mock*m=p;assert(m->locked && m->lease);if(reg==0xa8){assert(val==1 && m->read_pending);m->read_pending=0;return;}assert(reg==0xa0);m->commands++;if(val&0x80000000u)assert(val==(0x80000000u|((0x734u>>1)<<16)|4u));else {assert(val==((0x704u>>1)<<16));m->read_pending=1;}}
static void init(Mock*m,struct a04_pwrap_owner*o,A04_USB_VCDT_PWRAP*a){memset(m,0,sizeof(*m));memset(o,0,sizeof(*o));memset(a,0,sizeof(*a));o->lock_context=m;o->lock=lock;o->unlock=unlock;o->online=1;o->bus.context=m;o->bus.read32=read32;o->bus.write32=write32;o->bus.delay=delay;o->bus.attempts=3;a->owner=o;a->claim_context=m;a->claim=claim;a->release=release;a->delay_us=delay_us;}
int main(void){Mock m;struct a04_pwrap_owner o;A04_USB_VCDT_PWRAP a;A04_MT6392_VCDT_SAMPLE s={7,8,9};
 init(&m,&o,&a);m.denied=1;assert(A04UsbVcdtPwrapSample(&a,&s)==-1 && !m.commands && !m.lease && !m.locked && m.unlocks==1 && s.raw==7);
 m.denied=0;assert(A04UsbVcdtPwrapSample(&a,&s)==0 && s.raw==1200 && s.adc_mv==527 && s.charger_mv==4986 && m.commands==3 && !m.lease && !m.locked && m.releases==1);
 init(&m,&o,&a);m.timeout=1;s.raw=7;assert(A04UsbVcdtPwrapSample(&a,&s)==-2 && o.bus.poisoned && m.lease && !m.locked && a.adc.held && a.adc.uncertain && !m.releases && s.raw==7);assert(A04UsbVcdtPwrapSample(&a,&s)==-1 && m.unlocks==2);
 init(&m,&o,&a);m.fail_release=1;s.raw=7;assert(A04UsbVcdtPwrapSample(&a,&s)==-2 && m.lease && !m.locked && a.adc.held && a.adc.uncertain && s.raw==7);
 init(&m,&o,&a);o.online=0;assert(A04UsbVcdtPwrapSample(&a,&s)==-1 && !m.commands && !m.lease && !m.locked);
 return 0;}
