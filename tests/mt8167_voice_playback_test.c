#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "voice_playback.h"
struct fake { uint32_t ana[5],afe[12]; int muted,admit,fail,steps; int16_t initial; char order[16]; };
static int ar(void *p,unsigned o,uint32_t *v){struct fake *f=p;if(o<0x700||o>0x710||(o&3))return -1;*v=f->ana[(o-0x700)/4];return 0;}
static int aw(void *p,unsigned o,uint32_t v){struct fake *f=p;if(o<0x700||o>0x710||(o&3))return -1;f->ana[(o-0x700)/4]=v;return 0;}
static int fr(void *p,unsigned o,uint32_t *v){struct fake *f=p;if(o<0xf00||o>0xf2c||(o&3))return -1;*v=f->afe[(o-0xf00)/4];return 0;}
static int fw(void *p,unsigned o,uint32_t v){struct fake *f=p;if(o<0xf00||o>0xf2c||(o&3))return -1;f->afe[(o-0xf00)/4]=v;return 0;}
static int delay(void *p,unsigned u){(void)p;(void)u;return 0;}
static int verify(void *p,int16_t *initial,int16_t *v,unsigned *cap){struct fake *f=p;if(!f->muted)return -1;*initial=f->initial;*v=127;*cap=0;return 0;}
static int step(struct fake *f,char c){f->order[f->steps++]=c;return f->fail==c?-1:0;}
static int pre(void *p){struct fake *f=p;return !f->admit;}
static int start(void *p){struct fake *f=p;assert((f->ana[2]&(1u<<8)) && f->muted);return step(f,'D');}
static int stop(void *p){return step(p,'S');}
static int unmute(void *p){struct fake *f=p;int r=step(f,'U');if(!r)f->muted=0;return r;}
static int mute(void *p){struct fake *f=p;int r=step(f,'M');if(!r)f->muted=1;return r;}
int main(void){struct fake f;struct a04_voice_playback s;struct a04_voice_playback_io io={{ {&f,ar,aw},{&f,fr,fw,delay},verify},&f,pre,start,stop,unmute,mute};
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.muted=1;
 {struct a04_codec_domain domain={0};assert(!a04_codec_domain_start(&domain,&io.codec.analog));assert(!a04_codec_domain_stop(&domain,&io.codec.analog));}
 assert(a04_voice_playback_start(&s,&io) && s.state==A04_VOICE_READY && !f.steps);
 f.admit=1;f.initial=37;assert(!a04_voice_playback_start(&s,&io));assert(s.voice.initial==37);assert(!a04_voice_playback_stop(&s,&io));
 assert(!strcmp(f.order,"DUMS") && s.state==A04_VOICE_RETAINED && !(f.ana[2]&(1u<<8)));
 assert(a04_voice_playback_start(&s,&io));
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.admit=1;f.muted=1;f.fail='U';
 assert(a04_voice_playback_start(&s,&io) && !strcmp(f.order,"DUMS") && s.state==A04_VOICE_RETAINED);
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.admit=1;f.muted=1;f.fail='D';
 assert(a04_voice_playback_start(&s,&io) && !strcmp(f.order,"DS") && s.state==A04_VOICE_RETAINED);
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.admit=1;f.muted=1;
 assert(!a04_voice_playback_start(&s,&io));f.fail='M';assert(a04_voice_playback_stop(&s,&io));
 assert(!strcmp(f.order,"DUMS") && (f.ana[2]&(1u<<8)) && s.state==A04_VOICE_RETAINED);
 return 0;}
