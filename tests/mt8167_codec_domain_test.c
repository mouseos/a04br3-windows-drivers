#include <assert.h>
#include <string.h>
#include "codec_domain.h"
struct fake { uint32_t v[5];unsigned calls,fail,writes; };
static int idx(unsigned offset) { return offset==0x704?0:offset==0x70c?1:offset==0x710?2:offset==0x700?3:offset==0x708?4:-1; }
static int rd(void *p,unsigned off,uint32_t *v){struct fake*f=p;int n=idx(off);assert(n>=0);if(++f->calls==f->fail)return -1;*v=f->v[n];return 0;}
static int wr(void *p,unsigned off,uint32_t v){struct fake*f=p;int n=idx(off);assert(n>=0);if(++f->calls==f->fail)return -1;f->v[n]=v;f->writes++;return 0;}
int main(void)
{
 struct fake f={{0x11,0x22,0x33,0x44,0x55},0,0,0};struct a04_codec_domain s={{0},0,0};struct a04_codec_domain_io io={&f,rd,wr};unsigned n;
 assert(!a04_codec_domain_start(&s,&io) && s.held && (f.v[0]&(1u<<20)) && (f.v[1]&(1u<<30)) && (f.v[2]&(1u<<15)) && (f.v[3]&((1u<<19)|2))==((1u<<19)|2) && (f.v[0]&(1u<<13)) && (f.v[4]&((1u<<16)|(1u<<17)|(1u<<27)))==((1u<<16)|(1u<<17)|(1u<<27)));
 assert(!a04_codec_domain_stop(&s,&io) && !s.held && f.v[0]==0x11 && f.v[1]==0x22 && f.v[2]==0x33 && f.v[3]==0x44 && f.v[4]==0x55);
 for(n=9;n<=32;n++) {memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.fail=n;
  assert(a04_codec_domain_start(&s,&io) && s.held && s.poisoned);assert(a04_codec_domain_stop(&s,&io));}
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);assert(!a04_codec_domain_start(&s,&io));f.v[1]&=~(1u<<30);
 assert(a04_codec_domain_stop(&s,&io) && s.held && s.poisoned);
 return 0;
}
