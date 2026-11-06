#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "codec_adc_session.h"
struct state { unsigned calls; int fail; };
static int pre(void *p){struct state*s=p;s->calls=s->calls*10+1;return s->fail==1;}
static int clockon(void *p){struct state*s=p;s->calls=s->calls*10+2;return s->fail==2;}
static int capture(void *p,struct a04_codec_adc_samples *o){struct state*s=p;s->calls=s->calls*10+3;o->raw[0]=123;return s->fail==3;}
static int quiet(void *p){struct state*s=p;s->calls=s->calls*10+4;return s->fail==4;}
int main(void){struct state s={0};struct a04_codec_adc_samples out,before;struct a04_codec_adc_session_io io={&s,pre,clockon,capture,quiet};unsigned i;
 memset(&out,0,sizeof(out));before=out;
 for(i=1;i<=4;i++){s.calls=0;s.fail=i;assert(a04_codec_adc_session(&io,&out)==(i==1?A04_ADC_REJECTED:A04_ADC_UNCERTAIN));assert(!memcmp(&out,&before,sizeof out));assert(s.calls==(i==1?1u:i==2?12u:i==3?123u:1234u));}
 s.calls=0;s.fail=0;assert(a04_codec_adc_session(&io,&out)==A04_ADC_COMPLETE);assert(s.calls==1234 && out.raw[0]==123);
 puts("ADC session ordering and failure retention: OK");return 0;}
