#include <assert.h>
#include <string.h>
#include "usb_owner.h"
typedef struct { unsigned gpio[256],top[64],pmic,stage,depth,ops,good,fail; } M;
static int lock(void*p){M*m=p;assert(!m->depth);m->depth=1;return 0;}
static void unlock(void*p){M*m=p;assert(m->depth);m->depth=0;}
static int verify(void*p){M*m=p;assert(m->depth);return m->good?0:-1;}
static unsigned tr(void*p,unsigned o){M*m=p;assert(m->depth);return m->top[o/4];}
static void tw(void*p,unsigned o,unsigned v){M*m=p;assert(m->depth);m->ops++;if(o==0x84)m->top[0x24/4]&=~v;else if(o==0x9c)m->top[0x3c/4]&=~v;else assert(0);}
static unsigned gr(void*p,unsigned o){M*m=p;assert(m->depth);return m->gpio[o/4];}
static void gw(void*p,unsigned o,unsigned v){M*m=p;assert(m->depth);m->ops++;if(o==0x118)m->gpio[0x110/4]&=~v;else if(o==0x114)m->gpio[0x110/4]|=v;else if(o==0x14)m->gpio[0x10/4]|=v;else assert(0);}
static uint32_t pr(void*p,unsigned o){M*m=p;assert(m->depth);if(o==0x9c)return 1;assert(o==0xa4);return m->stage?(6u<<16)|m->pmic:0;}
static void pw(void*p,unsigned o,uint32_t v){M*m=p;assert(m->depth);m->ops++;if(o==0xa8){m->stage=0;return;}assert(o==0xa0);if(v>>31)m->pmic=v&0xffff;else m->stage=1;}
static void delay(void*p){(void)p;}
int main(void){M m;struct a04_pmic_read bus;struct a04_usb_owner o;memset(&m,0,sizeof(m));memset(&o,0,sizeof(o));
 bus=(struct a04_pmic_read){&m,pr,pw,delay,4,0};o.context=&m;o.lock=lock;o.unlock=unlock;o.verify_upstream=verify;o.verify_overcurrent=verify;o.top_read=tr;o.top_write=tw;o.pmic=&bus;o.gpio=(A04_GPIO_IO){&m,gr,gw};o.online=1;
 assert(a04_usb_owner_acquire(&o)<0 && o.poisoned && m.ops==0);
 o.poisoned=0;m.good=1;m.top[0x24/4]=1u<<13;m.top[0x3c/4]=(1u<<24)|(1u<<31);m.pmic=0x11;
 assert(!a04_usb_owner_acquire(&o) && o.leased && m.pmic==0x4011);
 assert(!a04_usb_owner_vbus(&o,1) && (m.gpio[0x110/4]&64));assert(a04_usb_owner_stop(&o)<0);
 assert(a04_usb_owner_vbus(&o,1)<0);assert(!a04_usb_owner_vbus(&o,0));assert(!a04_usb_owner_release(&o));assert(!a04_usb_owner_stop(&o));
 assert(m.pmic==0x4011 && !(m.gpio[0x110/4]&64));return 0;}
