#include <assert.h>
#include <stdint.h>
#include "clock_owner.h"
struct context { int lock, fail, checks; };
static int take(void *v){struct context*c=v;assert(!c->lock);c->lock=1;return 0;}
static void drop(void *v){struct context*c=v;assert(c->lock);c->lock=0;}
static int verify(void *v,uint32_t r){struct context*c=v;assert(c->lock && r);c->checks++;return c->fail;}
int main(void){struct context c={0};struct mt8167_clock_owner o={&c,take,drop,verify,0,0,1,0};
 assert(!mt8167_clock_acquire(&o,0,48000));assert(!mt8167_clock_acquire(&o,1,48000));
 assert(mt8167_clock_acquire(&o,2,44100)==-1 && c.checks==2);
 assert(mt8167_clock_acquire(&o,1,48000)==-1);
 assert(mt8167_clock_stop(&o)==-1 && !o.online);
 assert(mt8167_clock_acquire(&o,2,48000)==-1);
 assert(!mt8167_clock_release(&o,0));assert(!mt8167_clock_release(&o,1));assert(!mt8167_clock_stop(&o));
 o.online=1;c.fail=1;assert(mt8167_clock_acquire(&o,2,48000)==-1 && o.poisoned && !o.clients);
 c.fail=0;assert(mt8167_clock_acquire(&o,2,48000)==-1);
}
