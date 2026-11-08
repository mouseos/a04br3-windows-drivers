/* SPDX-License-Identifier: MIT. Actual production wait policy; no GPU. */
#include <assert.h>
#include <stdio.h>
#include "../drivers/gpu/mt8167/wddm-render/umd/readback_wait.h"
struct fixture {uint32_t now,calls,pauses,succeed_at;int32_t error;};
static int32_t attempt(void *p){struct fixture *f=p;f->calls++;return f->succeed_at&&f->calls>=f->succeed_at?0:f->error;}
static uint32_t clock_ms(void *p){return ((struct fixture *)p)->now;}
static void pause_ms(void *p){struct fixture *f=p;f->pauses++;f->now+=100;}
int main(void){
 struct fixture f={0,0,0,4,(int32_t)0x8876021c};struct geo_readback_wait_ops o={&f,attempt,clock_ms,pause_ms};
 assert(!geo_readback_wait(&o,f.error,(int32_t)0x887a000a,0)&&f.calls==4&&f.pauses==3);
 f=(struct fixture){0,0,0,0,(int32_t)0x8876021c};assert(geo_readback_wait(&o,f.error,(int32_t)0x887a000a,1)==f.error&&f.calls==1&&!f.pauses);
 f=(struct fixture){UINT32_MAX-100,0,0,0,(int32_t)0x8876021c};assert(geo_readback_wait(&o,f.error,(int32_t)0x887a000a,0)==GEO_READBACK_DEVICE_HUNG&&f.calls==301&&f.pauses==300);
 f=(struct fixture){0,0,0,0,(int32_t)0x80004005};assert(geo_readback_wait(&o,(int32_t)0x8876021c,(int32_t)0x887a000a,0)==f.error&&f.calls==1&&!f.pauses);
 puts("production Lock wait: real-success only / immediate nonblocking / wraparound deadline / real failure preserved PASS");
}
