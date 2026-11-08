/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../drivers/bus/mt8167-shared/codec_headphone.h"
struct fixture{uint32_t analog[2],afe[4],original[6];unsigned calls,fail,writes,delays;int qualified,drained;};
static int fault(struct fixture*f){return ++f->calls==f->fail;}
static int qualify(void*p,uint32_t generation){struct fixture*f=p;return fault(f)||!f->qualified||generation!=77;}
static uint32_t *reg(struct fixture*f,unsigned bank,unsigned off){
 if(!bank){assert(off==0x704||off==0x708);return &f->analog[(off-0x704)/4];}
 assert(bank==1);switch(off){case 0xf00:return &f->afe[0];case 0xf0c:return &f->afe[1];case 0xf10:return &f->afe[2];case 0xf2c:return &f->afe[3];default:assert(0);return 0;}
}
static int read32(void*p,unsigned bank,unsigned off,uint32_t*v){struct fixture*f=p;if(fault(f))return -1;*v=*reg(f,bank,off);return 0;}
static int write32(void*p,unsigned bank,unsigned off,uint32_t v){struct fixture*f=p;f->writes++;*reg(f,bank,off)=v;if(off==0xf2c)*reg(f,bank,off)=(v&~2u)|((v&0x200u)?2u:0);return fault(f)?-1:0;}
static int wait_us(void*p,unsigned us){struct fixture*f=p;assert(us==75||us==125||us==400||us==2000);f->delays++;return fault(f)?-1:0;}
static int drain(void*p){struct fixture*f=p;return fault(f)||!f->drained;}
static struct fixture initial(void){struct fixture f={0};f.qualified=f.drained=1;f.analog[0]=(1u<<13)|0x1b|(2u<<6);f.analog[1]=(1u<<16)|(1u<<17)|(1u<<27)|(9u<<9)|16;f.afe[0]=2;f.afe[1]=0xabcd0007;f.afe[2]=0x3456fff8;f.afe[3]=0x100;memcpy(f.original,f.analog,8);memcpy(f.original+2,f.afe,16);return f;}
int main(void){
 struct fixture f=initial();struct a04_hp_io io={&f,qualify,read32,write32,wait_us,drain};
 struct a04_hp_measurements measured={77,3,1,{0,3},{{700000,699000,698000},{700000,701000,702000}}};
 struct a04_hp_calibration c={0},unchanged={0};assert(a04_codec_headphone_calibration(&measured,&c)==0&&c.vcm[0]==0x4b4b&&c.dac[0]==-48&&c.op[0]==-97&&c.dac[1]==24&&c.op[1]==48);
 struct a04_hp_measurements bad=measured;bad.measured_channels=1;unchanged=c;assert(a04_codec_headphone_calibration(&bad,&c)==-1&&!memcmp(&c,&unchanged,sizeof(c)));
 bad=measured;bad.uv[1][2]=1500001;assert(a04_codec_headphone_calibration(&bad,&c)==-1&&!memcmp(&c,&unchanged,sizeof(c)));
 struct a04_hp_route s={0};assert(a04_codec_headphone_start(&s,&io,&c)==0&&s.active&&s.held&&!s.poisoned);
 unsigned start_calls=f.calls;assert((f.analog[0]&(3u<<11|3u<<14))==(3u<<11|3u<<14));assert(!(f.analog[1]&(1u<<8)));assert((f.afe[1]&0xffff)==(uint16_t)c.op[0]&&(f.afe[2]&0xffff)==(uint16_t)c.op[1]);
 f.drained=0;unsigned writes=f.writes;assert(a04_codec_headphone_stop(&s,&io)==-1&&f.writes==writes&&s.held);f.drained=1;
 f.calls=0;assert(a04_codec_headphone_stop(&s,&io)==0&&!s.held&&!s.active);unsigned stop_calls=f.calls;
 assert(!memcmp(f.original,f.analog,8)&&!memcmp(f.original+2,f.afe,12));assert((f.afe[3]&~0x202u)==(f.original[5]&~0x202u));
 /* Every fault position: either reject before writes or retain poisoned owner.
  * A failed write may already have reached hardware: no automatic rollback. */
 for(unsigned fail=1;fail<=start_calls;fail++){
  f=initial();f.fail=fail;memset(&s,0,sizeof(s));int result=a04_codec_headphone_start(&s,&io,&c);assert(result<0);
  if(result==-1)assert(!f.writes&&!s.held);else{assert(s.held&&s.poisoned);writes=f.writes;assert(a04_codec_headphone_stop(&s,&io)==-1&&f.writes==writes);}
 }
 for(unsigned fail=1;fail<=stop_calls;fail++){
  f=initial();memset(&s,0,sizeof(s));assert(a04_codec_headphone_start(&s,&io,&c)==0);f.calls=0;f.fail=fail;
  assert(a04_codec_headphone_stop(&s,&io)<0&&s.held);if(s.poisoned){writes=f.writes;assert(a04_codec_headphone_stop(&s,&io)==-1&&f.writes==writes);}
 }
 /* No calibration, active voice/DAC, lease loss and missing independent drain. */
 f=initial();memset(&s,0,sizeof(s));struct a04_hp_calibration empty={0};assert(a04_codec_headphone_start(&s,&io,&empty)==-1&&!f.writes);
 for(unsigned route=0;route<3;route++){f=initial();memset(&s,0,sizeof(s));if(route==0)f.analog[0]|=1u<<15;if(route==1)f.analog[1]|=1u<<8;if(route==2)f.afe[0]|=1;assert(a04_codec_headphone_start(&s,&io,&c)==-1&&!f.writes);}
 printf("HP route: %u start + %u stop fault positions, stereo latch/ramp/restore and no-write guards PASS\n",start_calls,stop_calls);
}
