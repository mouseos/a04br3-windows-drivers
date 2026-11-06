#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "codec_voice.h"
struct fake { uint32_t ana[5],afe[12]; int allowed,writes,fail; };
static int ar(void *p,unsigned o,uint32_t *v){struct fake *f=p;if(o<0x700||o>0x710)return -1;*v=f->ana[(o-0x700)/4];return 0;}
static int aw(void *p,unsigned o,uint32_t v){struct fake *f=p;if(++f->writes==f->fail)return -1;f->ana[(o-0x700)/4]=v;return 0;}
static int fr(void *p,unsigned o,uint32_t *v){struct fake *f=p;if(o<0xf00||o>0xf2c)return -1;*v=f->afe[(o-0xf00)/4];return 0;}
static int fw(void *p,unsigned o,uint32_t v){struct fake *f=p;if(++f->writes==f->fail)return -1;f->afe[(o-0xf00)/4]=v;return 0;}
static int delay(void *p,unsigned u){(void)p;(void)u;return 0;}
static int verify(void *p,int16_t *initial,int16_t *v,unsigned *cap){struct fake *f=p;if(!f->allowed)return -1;*initial=0;*v=127;*cap=0;return 0;}
int main(void){struct fake f;struct a04_codec_voice s;struct a04_codec_domain d;struct a04_codec_voice_io io={{&f,ar,aw},{&f,fr,fw,delay},verify};
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);memset(&d,0,sizeof d);
 assert(!a04_codec_domain_start(&d,&io.analog));assert(!a04_codec_domain_stop(&d,&io.analog));f.writes=0;
 assert(a04_codec_voice_start(&s,&io) && !f.writes);f.allowed=1;f.ana[2]=4u<<9;
 assert(!a04_codec_voice_start(&s,&io));assert(s.active && (f.ana[1]&(1u<<15)) && (f.ana[2]&(1u<<8)) && (f.afe[0]&1));
 f.allowed=0;assert(a04_codec_voice_stop(&s,&io) && s.active);f.allowed=1;
 assert(!a04_codec_voice_stop(&s,&io));assert(!s.active && !(f.ana[1]&(1u<<15)) && !(f.ana[2]&(1u<<8)) && !(f.afe[0]&1) && (f.ana[2]&(15u<<9))==(4u<<9));
 memset(&s,0,sizeof s);f.fail=f.writes+1;assert(a04_codec_voice_start(&s,&io) && s.poisoned && s.active);assert(a04_codec_voice_stop(&s,&io));return 0;}
