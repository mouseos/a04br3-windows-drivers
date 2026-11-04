/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "lifetime.h"
struct fake {unsigned seq,irq,drain,unmap,release,requests;int fail_at;};
static int irq(void *p){struct fake *f=p;f->irq=++f->seq;return f->fail_at==1;}
static int drain(void *p){struct fake *f=p;f->drain=++f->seq;return f->fail_at==2;}
static int unmap(void *p){struct fake *f=p;f->unmap=++f->seq;return f->fail_at==3;}
static int release(void *p){struct fake *f=p;f->release=++f->seq;return f->fail_at==4;}
static int direct(void *p,uint32_t a,uint32_t *r){struct fake *f=p;(void)a;f->requests++;*r=0x42;return 0;}
static int transfer(void *p,uint32_t a,unsigned w,uint8_t *data,size_t n){struct fake *f=p;(void)a;(void)w;(void)data;(void)n;f->requests++;return 0;}
int main(void)
{
 uint8_t cis[]={0x20,4,0x7a,0x03,0x30,0x66,0xff},v=0;
 struct a04_wifi_owner o={0,1,1,1,1,1,0,0};struct fake f={0};struct a04_wifi_life s={0};
 struct a04_sdio_direct d={&o,&f,direct,0,0};struct a04_sdio_bus bus={&f,transfer};
 struct a04_wifi_life_ops ops={&f,irq,drain,unmap,release};
 assert(!a04_wifi_admit(&o));assert(!a04_wifi_life_begin(&s,&o,cis,sizeof(cis),&d,&bus,&ops));
 assert(!a04_wifi_life_cmd52(&s,0,1,0,0,&v)&&v==0x42);assert(!a04_wifi_life_cmd53(&s,0,1,1,0,&v,1));
 assert(!a04_wifi_life_stop(&s));assert(f.irq==1&&f.drain==2&&f.unmap==3&&f.release==4);
 assert(a04_wifi_life_cmd52(&s,0,1,0,0,&v)<0 && f.requests==2);assert(a04_wifi_life_stop(&s)<0);
 for(int fail=1;fail<=4;fail++){
  o=(struct a04_wifi_owner){0,1,1,1,1,1,0,0};f=(struct fake){0};s=(struct a04_wifi_life){0};f.fail_at=fail;
  assert(!a04_wifi_admit(&o));assert(!a04_wifi_life_begin(&s,&o,cis,sizeof(cis),&d,&bus,&ops));
  assert(a04_wifi_life_stop(&s)==-2 && s.state==A04_WIFI_LIFE_STOP_FAILED && o.poisoned);
  assert(f.seq==(unsigned)fail && a04_wifi_life_cmd53(&s,0,1,1,0,&v,1)<0);
  assert(a04_wifi_life_stop(&s)==-1 && f.seq==(unsigned)fail);
 }
 puts("SDIO admission, IRQ drain and teardown ordering: OK");return 0;
}
