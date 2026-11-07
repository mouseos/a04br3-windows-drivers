/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <string.h>
#include "auxadc_pio.h"
struct bus { unsigned reads,writes,waits,ops,fail,stuck_busy,old_ready,new_missing,lost;uint16_t trigger; };
static int qualified(void *p){return !((struct bus *)p)->lost;}
static int rd(void *p,unsigned reg,uint16_t *v){struct bus *b=p;b->reads++;if(++b->ops==b->fail)return -1;
 *v=reg==4?b->trigger:reg==0x10?(b->stuck_busy && b->writes?1:0):reg==0x40?(b->trigger?(b->new_missing?0:0x1abcu):(b->old_ready?0x1abcu:0)):0;return 0;}
static int wr(void *p,unsigned reg,uint16_t v){struct bus *b=p;assert(reg==4);b->writes++;b->trigger=v;if(++b->ops==b->fail)return -1;return 0;}
static int delay(void *p,unsigned us){struct bus *b=p;assert(us==25||us==1000);b->waits++;return ++b->ops==b->fail?-1:0;}
static enum a04_auxadc_pio_result run(struct bus *b,struct a04_auxadc_pio_state *s,uint16_t *raw){struct a04_auxadc_pio_io io={b,qualified,rd,wr,delay};return a04_auxadc_pio_channel11(s,&io,raw);}
int main(void){struct bus b={0};struct a04_auxadc_pio_state s={0};uint16_t raw=0xbeef;unsigned ops,i;
 assert(run(&b,&s,&raw)==A04_PIO_OK&&raw==0xabc&&!b.trigger&&!s.touched);ops=b.ops;
 /* Inject failure at every read/write/wait of the successful transaction.
  * In particular an accepted-but-reported-failed trigger write retains. */
 for(i=1;i<=ops;i++){memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.fail=i;raw=0xbeef;
  assert(run(&b,&s,&raw)!=A04_PIO_OK&&raw==0xbeef);
  if(s.quarantined){unsigned before=b.ops;assert(run(&b,&s,&raw)==A04_PIO_REJECTED&&b.ops==before);}}
 memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.old_ready=1;raw=0xbeef;
 assert(run(&b,&s,&raw)==A04_PIO_SAMPLE_FAILED_ADC_IDLE&&raw==0xbeef&&b.waits<=31&&!s.touched);
 memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.new_missing=1;
 assert(run(&b,&s,&raw)==A04_PIO_SAMPLE_FAILED_ADC_IDLE&&!s.quarantined&&!s.touched);
 memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.stuck_busy=1;
 assert(run(&b,&s,&raw)==A04_PIO_RETAIN&&s.quarantined&&s.touched);
 memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.lost=1;
 assert(run(&b,&s,&raw)==A04_PIO_REJECTED&&!b.ops);
 return 0;}
