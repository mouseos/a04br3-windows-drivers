/* SPDX-License-Identifier: GPL-2.0-only */
#include "completion.h"
#define T 0x1100d000u
int a04_completion_start(struct a04_completion *c,uint64_t now,uint64_t timeout)
{
 uint32_t marker[3]={0xda1,0xda1,0xda1};
 if(!c || !timeout) return -1;
 *c=(struct a04_completion){0};c->start_ms=now;c->timeout_ms=timeout;c->initialized=1;
 (void)a04_freshness_update(&c->freshness,marker,now,timeout);return 0;
}
int a04_completion_poll(struct a04_completion *c,const struct a04_thermal_hw *h,uint64_t now,uint64_t max_age,uint32_t out[3])
{
 uint32_t ctl,pause,mux,en,valid,volt,bus,values[3];unsigned i;
 if(!c || !h || !out || !c->initialized || !h->qualified || !h->initialized || !h->read32 ||
    !h->qualified(h->ctx) || !h->initialized(h->ctx) || !max_age || now<c->start_ms) goto fail;
 if(h->read32(h->ctx,T+0,&ctl) || h->read32(h->ctx,T+0x03c,&pause) ||
    h->read32(h->ctx,T+0x068,&mux) || h->read32(h->ctx,T+0x074,&en) ||
    h->read32(h->ctx,T+0x078,&valid) || h->read32(h->ctx,T+0x07c,&volt) ||
    h->read32(h->ctx,T+0x418,&bus)) goto fail;
 if((ctl&7u)!=7u || (pause&0x0eu) || mux!=0x1100300cu || en!=0x11003008u ||
    valid!=0x11003040u || volt!=0x11003040u) goto fail;
 if(bus>>16) goto pending;
 for(i=0;i<3;i++) if(h->read32(h->ctx,T+0x90+4*i,&values[i]) || !(values[i]&0xfffu)) goto fail;
 if(!a04_freshness_update(&c->freshness,values,now,max_age)) {
  for(i=0;i<3;i++)out[i]=values[i];
  c->start_ms=now;
  return 1;
 }
pending:
 if(now-c->start_ms>c->timeout_ms) goto fail;
 return 0;
fail:
 if(c)c->initialized=0;
 return -1;
}
