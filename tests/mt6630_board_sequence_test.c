/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "board_sequence.h"
struct mock {uint32_t regs[3][1024];unsigned operations,fail_at,ms;uint32_t last_write[2];unsigned power_writes;int qualified;unsigned revoke_at;};
static int qualified(void *c){return ((struct mock *)c)->qualified;}
static int readreg(void *c,enum a04_wifi_board_bank b,unsigned off,uint32_t *v){struct mock *m=c;if(++m->operations==m->fail_at)return -1;if(m->operations==m->revoke_at)m->qualified=0;*v=m->regs[b][off/4];if(b==A04_WIFI_MSDC&&off==0){*v&=~4u;*v|=0x80;}return 0;}
static int writereg(void *c,enum a04_wifi_board_bank b,unsigned off,uint32_t v){struct mock *m=c;if(++m->operations==m->fail_at)return -1;if(m->operations==m->revoke_at)m->qualified=0;m->regs[b][off/4]=v;if(b==A04_WIFI_GPIO&&off==0x108&&m->power_writes<2)m->last_write[m->power_writes++]=v;return 0;}
static int update_shared(void *c,enum a04_wifi_board_bank b,unsigned off,uint32_t mask,uint32_t value){struct mock *m=c;if(++m->operations==m->fail_at)return -1;if(m->operations==m->revoke_at)m->qualified=0;m->regs[b][off/4]=(m->regs[b][off/4]&~mask)|value;return 0;}
static int delay(void *c,unsigned ms){struct mock *m=c;if(++m->operations==m->fail_at)return -1;if(m->operations==m->revoke_at)m->qualified=0;m->ms+=ms;return 0;}
int main(void){struct mock m={0};struct a04_wifi_board s={0};struct a04_wifi_board_io io={&m,qualified,readreg,writereg,delay,update_shared};
 assert(a04_wifi_board_start(&s,&io)<0&&!m.operations&&!s.started);m.qualified=1;
 assert(!a04_wifi_board_start(&s,&io)&&s.ready&&!s.poisoned&&m.ms==150);
 assert(a04_wifi_board_start(&s,&io)<0);
 {struct mock stop={0};struct a04_wifi_board t={1,1,0};stop.qualified=1;io.context=&stop;
  assert(!a04_wifi_board_poweroff_signals(&t,&io)&&!t.ready&&stop.power_writes==2);
  assert(stop.last_write[0]==(1u<<7)&&stop.last_write[1]==(1u<<4));
  assert(a04_wifi_board_poweroff_signals(&t,&io)&&stop.operations==2);
  t.ready=1;stop.qualified=0;assert(a04_wifi_board_poweroff_signals(&t,&io)&&stop.operations==2);
  stop.qualified=1;stop.fail_at=4;assert(a04_wifi_board_poweroff_signals(&t,&io)&&t.poisoned&&stop.operations==4);
 }
 io.context=&m;
 for(unsigned n=1;n<=m.operations;n++){struct mock f={0};struct a04_wifi_board t={0};f.qualified=1;f.fail_at=n;io.context=&f;assert(a04_wifi_board_start(&t,&io)<0&&t.poisoned&&!t.ready);assert(a04_wifi_board_start(&t,&io)<0);}
 for(unsigned n=1;n<=m.operations;n++){struct mock f={0};struct a04_wifi_board t={0};f.qualified=1;f.revoke_at=n;io.context=&f;assert(a04_wifi_board_start(&t,&io)<0&&t.poisoned&&!t.ready);assert(f.operations==n);}
 {struct mock f={0};struct a04_wifi_board t={0};f.qualified=-1;io.context=&f;assert(a04_wifi_board_start(&t,&io)<0&&!f.operations);}
 puts("MT6630 board sequence ownership gate and failure injection: OK");return 0;}
