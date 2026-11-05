#include <assert.h>
#include "audio_owner.h"
struct mock { int sequence, fail; };
static int pre(void *p,uint32_t rate){struct mock*m=p;assert(rate==48000);assert(m->sequence==0);m->sequence=1;return m->fail==1;}
static int clock_on(void *p,uint32_t rate){struct mock*m=p;assert(rate==48000 && m->sequence==1);m->sequence=2;return m->fail==2;}
static int codec_on(void *p){struct mock*m=p;assert(m->sequence==2);m->sequence=3;return m->fail==3;}
static int codec_off(void *p){struct mock*m=p;assert(m->sequence==3);m->sequence=4;return m->fail==4;}
static int clock_off(void *p){struct mock*m=p;assert(m->sequence==4);m->sequence=5;return m->fail==5;}
static struct a04_audio_owner owner(struct mock *m){struct a04_audio_owner o={m,pre,clock_on,codec_on,codec_off,clock_off,1,0,0};return o;}
int main(void){struct mock m={0};struct a04_audio_owner o=owner(&m);int n;
 assert(!a04_audio_owner_acquire(&o,48000) && o.leased && m.sequence==3);
 assert(a04_audio_owner_acquire(&o,48000));assert(!a04_audio_owner_release(&o));assert(!o.leased && m.sequence==5);
 for(n=1;n<=3;n++){m.sequence=0;m.fail=n;o=owner(&m);assert(a04_audio_owner_acquire(&o,48000));assert(!o.leased);assert(o.poisoned==(n!=1));assert(m.sequence==n);}
 for(n=4;n<=5;n++){m.sequence=0;m.fail=0;o=owner(&m);assert(!a04_audio_owner_acquire(&o,48000));m.fail=n;assert(a04_audio_owner_release(&o));assert(o.leased && o.poisoned && m.sequence==n);assert(a04_audio_owner_release(&o));}
 m.sequence=0;m.fail=0;o=owner(&m);o.preflight=0;assert(a04_audio_owner_acquire(&o,48000) && !o.poisoned && !m.sequence);
 o=owner(&m);assert(!a04_audio_owner_acquire(&o,48000));a04_audio_owner_stop(&o);assert(o.leased && o.poisoned && !o.online);assert(a04_audio_owner_release(&o));
 return 0;
}
