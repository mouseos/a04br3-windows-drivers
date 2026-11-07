#include <assert.h>
#include <stdint.h>
#include "register_owner.h"
#define BIT (1u<<25)
struct mock { uint32_t status; int reads,writes,fail_read,fail_write,ignore; };
static int rd(void *p,enum a04_shared_bank b,unsigned off,uint32_t *v)
{
    struct mock *m=p;assert(b==A04_SHARED_TOP && off==0x24);m->reads++;
    if(m->fail_read==m->reads)return -1;
    *v=m->status;return 0;
}
static int wr(void *p,enum a04_shared_bank b,unsigned off,uint32_t v)
{
    struct mock *m=p;assert(b==A04_SHARED_TOP && v==BIT);m->writes++;
    if(m->fail_write==m->writes)return -1;
    if(!m->ignore){if(off==0x84)m->status&=~BIT;else {assert(off==0x54);m->status|=BIT;}}
    return 0;
}
int main(void)
{
    struct mock m={0};struct a04_register_owner o={0};struct a04_audio_top_clock s={0};
    o.context=&m;o.read32=rd;o.write32=wr;
    m.status=BIT|8;assert(!a04_audio_top_clock_acquire(&o,&s) && s.held && m.status==8 && m.writes==1);
    assert(a04_audio_top_clock_acquire(&o,&s));assert(!a04_audio_top_clock_release(&o,&s) && !s.held && m.status==(BIT|8) && m.writes==2);
    m.status=8;m.writes=0;assert(!a04_audio_top_clock_acquire(&o,&s));assert(!a04_audio_top_clock_release(&o,&s) && !m.writes);
    m.status=BIT;m.ignore=1;assert(a04_audio_top_clock_acquire(&o,&s) && s.held && s.poisoned);assert(a04_audio_top_clock_release(&o,&s));
    s=(struct a04_audio_top_clock){0};m=(struct mock){0};m.status=BIT;m.fail_write=1;
    assert(a04_audio_top_clock_acquire(&o,&s) && s.held && s.poisoned);
    s=(struct a04_audio_top_clock){0};m=(struct mock){0};m.status=BIT;assert(!a04_audio_top_clock_acquire(&o,&s));m.status|=BIT;
    assert(a04_audio_top_clock_release(&o,&s) && s.held && s.poisoned && m.writes==1);
    return 0;
}
