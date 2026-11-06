#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codec_adc_probe.h"
#include "codec_adc_session.h"
struct mock { uint16_t con; unsigned writes,reads,samples,ready_reads; int qualified,stuck,lose_ready; };
static int qual(void *p) {return ((struct mock *)p)->qualified;}
static int rd(void *p,uint32_t reg,uint16_t *out) {
 struct mock *m=p;m->reads++;
 if(reg==4)*out=m->con;
 else if(reg==0x10)*out=0;
 else if(reg==0x30 || reg==0x34) {
  int ready=(m->con & (reg==0x30?128:256)) && !m->stuck;
  if(ready && m->lose_ready && ++m->ready_reads==2)ready=0;
  *out=ready ? (uint16_t)(0x1000|(1000+m->samples)) : 0;
 }
 else return -1;
 return 0;
}
static int wr(void *p,uint32_t reg,uint16_t val) {
 struct mock *m=p;if(reg!=4)return -1;
 if((m->con&128) && !(val&128))m->samples++;
 m->con=val;m->writes++;return 0;
}
static int delay(void *p,unsigned us) {(void)p;return us==25 || us==1000?0:-1;}
struct chain { struct mock m;struct a04_codec_adc_probe probe;int clock,quiescent; };
static int chain_pre(void *p) {return !((struct chain *)p)->m.qualified;}
static int chain_clock(void *p) {((struct chain *)p)->clock=1;return 0;}
static int chain_capture(void *p,struct a04_codec_adc_samples *out) {
 struct chain *c=p;struct a04_codec_adc_io io={&c->m,qual,rd,wr,delay};
 return !c->clock || a04_codec_adc_capture(&c->probe,&io,7,out);
}
static int chain_quiesce(void *p) {struct chain *c=p;if(c->m.con)return -1;c->quiescent=1;return 0;}
int main(void) {
 struct mock m={0};struct a04_codec_adc_probe s={0};struct a04_codec_adc_samples out,old;
 struct a04_codec_adc_io io={&m,qual,rd,wr,delay};
 memset(&out,0xa5,sizeof(out));old=out;
 assert(a04_codec_adc_capture(&s,&io,7,&out));assert(!m.reads && !m.writes && !memcmp(&out,&old,sizeof(out)));
 m.qualified=1;assert(!a04_codec_adc_capture(&s,&io,7,&out));
 assert(m.samples==22 && m.writes==44 && !m.con && out.raw[0]==1000 && out.raw[21]==1021 && out.average_uv==370055);
 memset(&out,0xa5,sizeof(out));old=out;m.stuck=1;
 assert(a04_codec_adc_capture(&s,&io,7,&out));assert(s.poisoned && !memcmp(&out,&old,sizeof(out)));
 {unsigned writes=m.writes;assert(a04_codec_adc_capture(&s,&io,7,&out));assert(m.writes==writes);}
 {struct mock lost={0};struct a04_codec_adc_probe state={0};
  struct a04_codec_adc_io lost_io={&lost,qual,rd,wr,delay};
  lost.qualified=1;lost.lose_ready=1;memset(&out,0xa5,sizeof(out));old=out;
  assert(a04_codec_adc_capture(&state,&lost_io,7,&out));
  assert(state.poisoned && lost.writes==1 && lost.con==128 && !memcmp(&out,&old,sizeof(out)));}
 {struct chain c={0};struct a04_codec_adc_session_io session={&c,chain_pre,chain_clock,chain_capture,chain_quiesce};
  c.m.qualified=1;assert(a04_codec_adc_session(&session,&out)==A04_ADC_COMPLETE);
  assert(c.quiescent && c.m.samples==22 && out.raw[0]==1000 && out.raw[21]==1021);}
 puts("codec ADC gated acquisition: OK");return 0;
}
