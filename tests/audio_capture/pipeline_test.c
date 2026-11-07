/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One fixture crosses input owner, analog codec and VUL/IRQ2 engines.
 * MMIO simulation is not microphone/IRQ/bus-drain evidence. */
#include <assert.h>
#include <string.h>
#include "../../drivers/bus/mt8167-shared/capture_input_owner.h"
#include "../../drivers/audio/mt8167-afe/ul_engine.h"
struct mock {uint32_t a[0x800/4],f[0x1000/4];unsigned power,blocked,drained,writes,fail_write;};
static int ar(void *p,unsigned r,uint32_t*v){struct mock*m=p;assert(r<sizeof(m->a)&&!(r&3));*v=m->a[r/4];return 0;}
static int aw(void *p,unsigned r,uint32_t v){struct mock*m=p;assert(m->power&&r<sizeof(m->a)&&!(r&3));if(++m->writes==m->fail_write)return -1;m->a[r/4]=v;return 0;}
static int fr(void *p,unsigned r,uint32_t*v){struct mock*m=p;assert(r<sizeof(m->f)&&!(r&3));*v=m->f[r/4];return 0;}
static int fw(void *p,unsigned r,uint32_t v){struct mock*m=p;assert(m->power&&r<sizeof(m->f)&&!(r&3));if(++m->writes==m->fail_write)return -1;if(r==0x3a8)m->f[0x3a4/4]&=~v;else m->f[r/4]=v;return 0;}
static int reserve(void*p){struct mock*m=p;return m->blocked||m->power;}
static int unreserve(void*p){struct mock*m=p;assert(!m->power);return 0;}
static int power(void*p,unsigned rate){struct mock*m=p;assert(rate==48000&&!m->power);m->power=1;return 0;}
static int off(void*p){struct mock*m=p;assert(m->drained);m->power=0;return 0;}
static int drain(void*p){return !((struct mock*)p)->drained;}
static void init(struct mock*m,struct a04_capture_input_owner*o,struct mt8167_ul_engine*e){
 memset(m,0,sizeof(*m));memset(o,0,sizeof(*o));memset(e,0,sizeof(*e));
 /* Preserve inherited CH1/PGA gain and unrelated debug-free fields. */
 m->a[0x700/4]=(1u<<28)|(1u<<10)|(3u<<25)|(2u<<7);m->f[0xf04/4]=0x40;m->f[0]=1u<<24;
 o->context=m;o->reserve=reserve;o->unreserve=unreserve;o->power_acquire=power;o->power_release=off;o->verify_drained=drain;
 o->io=(struct a04_capture_codec_io){m,ar,aw,fr,fw};o->online=1;e->io=(struct mt8167_ul_io){m,fr,fw};
}
int main(void){struct mock m;struct a04_capture_input_owner o;struct mt8167_ul_engine e;uint32_t initial,position,notify;unsigned writes;
 init(&m,&o,&e);m.blocked=1;assert(a04_capture_input_acquire(&o,48000)==-1&&!m.writes&&!o.reserved);
 m.blocked=0;assert(a04_capture_input_acquire(&o,44100)==-1&&!m.writes);
 initial=m.a[0x700/4];assert(!a04_capture_input_acquire(&o,48000)&&o.ready&&o.reserved&&m.power);
 assert((m.a[0x700/4]&((3u<<28)|(3u<<10)))==0);assert((m.f[0xf04/4]&0xf0)==0x10);
 assert(!mt8167_ul_prepare(&e,0x88000000u,16384,8192));assert(e.exposed&&!e.running);
 assert(!mt8167_ul_start(&e));assert((m.f[0x14/4]&(15u<<16))==(10u<<16));
 assert(m.f[0x80/4]==0x88000000u&&m.f[0x88/4]==0x88003fffu);
 m.f[0x8c/4]=0x88000100u;assert(!mt8167_ul_position(&e,&position)&&position==256);
 m.f[0x3a4/4]=2;assert(mt8167_ul_interrupt(&e,&notify)==1&&notify==1&&!m.f[0x3a4/4]);
 assert(!mt8167_ul_stop(&e)&&e.exposed);writes=m.writes;
 assert(a04_capture_input_release(&o)==-1&&m.writes==writes&&o.reserved&&m.power);
 /* Same ring can resume; register stop did not certify page reclamation. */
 assert(!mt8167_ul_start(&e)&&!mt8167_ul_stop(&e));
 a04_capture_input_offline(&o);assert(a04_capture_input_acquire(&o,48000)==-1);
 m.drained=1;assert(!a04_capture_input_release(&o)&&!m.power&&!o.reserved);
 assert(m.a[0x700/4]==initial&&(m.f[0xf04/4]&0xf0)==0x40&&(m.f[0]&(1u<<24)));
 /* A write fault in the analog power transition retains the parent lease. */
 init(&m,&o,&e);m.fail_write=3;assert(a04_capture_input_acquire(&o,48000)==-2&&o.poisoned&&o.reserved&&m.power);
 assert(a04_capture_input_release(&o)==-1&&m.power);
 /* Unrelated active memif is never reset on capture admission. */
 init(&m,&o,&e);m.f[0x10/4]=2;assert(a04_capture_input_acquire(&o,48000)==-2&&!m.writes&&o.reserved);
 return 0;
}
