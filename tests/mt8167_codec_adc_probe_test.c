#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codec_adc_probe.h"
struct mock { uint16_t con; unsigned writes,reads,samples; int qualified,stuck; };
static int qual(void *p) {return ((struct mock *)p)->qualified;}
static int rd(void *p,uint32_t reg,uint16_t *out) {
 struct mock *m=p;m->reads++;
 if(reg==4)*out=m->con;
 else if(reg==0x10)*out=0;
 else if(reg==0x30 || reg==0x34)*out=(m->con & (reg==0x30?128:256)) && !m->stuck ? (uint16_t)(0x1000|(1000+m->samples)) : 0;
 else return -1;
 return 0;
}
static int wr(void *p,uint32_t reg,uint16_t val) {
 struct mock *m=p;if(reg!=4)return -1;
 if((m->con&128) && !(val&128))m->samples++;
 m->con=val;m->writes++;return 0;
}
static int delay(void *p,unsigned us) {(void)p;return us==25 || us==1000?0:-1;}
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
 puts("codec ADC gated acquisition: OK");return 0;
}
