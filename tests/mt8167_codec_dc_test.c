#include <assert.h>
#include <stdint.h>
#include "codec_dc.h"
struct mock {uint32_t comp,ctrl;unsigned writes,delays,fail;int16_t last;};
static int rd(void *c,unsigned o,uint32_t *v){struct mock*m=c;*v=o==0xf0c?m->comp:m->ctrl;return 0;}
static int wr(void *c,unsigned o,uint32_t v){struct mock*m=c;if(++m->writes==m->fail)return -1;if(o==0xf0c){m->comp=v;m->last=(int16_t)v;}else {m->ctrl=(v&~2u)|((v&0x200u)?2u:0u);}return 0;}
static int wait(void *c,unsigned us){struct mock*m=c;assert(us==400 || us==2000);m->delays++;return 0;}
int main(void){struct mock m={0};struct a04_codec_dc s={0};struct a04_codec_dc_io io={&m,rd,wr,wait};
 assert(!a04_codec_dc_ramp(&s,&io,-3000,3000,0));assert(m.last==3000 && m.delays==4 && !s.poisoned);
 assert(!a04_codec_dc_ramp(&s,&io,3000,-3000,3));assert(m.last==-3000 && m.delays==20);
 assert(a04_codec_dc_ramp(&s,&io,0,0,4));m.fail=m.writes+2;assert(a04_codec_dc_set(&s,&io,0) && s.poisoned);assert(a04_codec_dc_set(&s,&io,0));return 0;}
