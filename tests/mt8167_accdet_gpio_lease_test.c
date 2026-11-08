/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Production lease and socket code, MMIO model only; no hardware claim. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/input/mt8167-accdet/gpio_lease.h"
#include "../drivers/input/mt8167-accdet/status.h"
struct model {uint32_t reg[1024];unsigned writes;int fail_write,drop_write,fail_read;};
static int rd(void*p,unsigned off,uint32_t*v){struct model*m=p;if(m->fail_read)return -1;*v=m->reg[off/4];return 0;}
static int wr(void*p,unsigned off,uint32_t v){struct model*m=p;assert(off==0x10||off==0x330||off==0x910);++m->writes;if(m->fail_write)return -1;if(!m->drop_write)m->reg[off/4]=v;return 0;}
static struct model initial(void){struct model m={0};m.reg[0x10/4]=0x3c3;m.reg[0x510/4]=0xfc20;m.reg[0x610/4]=0x4020;m.reg[0x910/4]=0x12;m.reg[0x210/4]=8;return m;}
int main(void){
 struct model m=initial();struct mt8167_accdet_gpio_io io={&m,rd,wr};struct mt8167_accdet_gpio_lease s={0};struct mt8167_accdet_io sense={&m,rd};struct mt8167_accdet_socket jack;struct mt8167_accdet_status status;
 uint32_t saved[1024];memcpy(saved,m.reg,sizeof(saved));
 assert(!mt8167_accdet_gpio_acquire(&s,&io)&&s.held&&!s.poisoned&&m.writes==1);
 assert(m.reg[0x910/4]==(saved[0x910/4]|0x400));
 mt8167_accdet_read_status(&sense,&status);assert(!status.reasons&&status.valid==0xfff&&status.result==MT8167_ACCDET_OK);
 assert(!mt8167_accdet_read_socket(&sense,&jack)&&jack.qualified&&!jack.inserted);
 m.reg[0x210/4]=0;assert(!mt8167_accdet_read_socket(&sense,&jack)&&jack.inserted);m.reg[0x210/4]=8;
 assert(!mt8167_accdet_gpio_live(&s,&io));assert(mt8167_accdet_gpio_acquire(&s,&io));
 assert(!mt8167_accdet_gpio_release(&s,&io,1)&&!s.held&&m.writes==2&&!memcmp(saved,m.reg,sizeof(saved)));
 /* A fresh D0 may acquire again. Already-enabled IES incurs no write. */
 m.reg[0x910/4]|=0x400;m.writes=0;assert(!mt8167_accdet_gpio_acquire(&s,&io)&&!m.writes);assert(!mt8167_accdet_gpio_release(&s,&io,1)&&!m.writes);
 for(unsigned bad=0;bad<4;bad++){
  m=initial();memset(&s,0,sizeof(s));if(bad==0)m.reg[0x10/4]|=8;if(bad==1)m.reg[0x330/4]|=0x1000;if(bad==2)m.reg[0x510/4]|=8;if(bad==3)m.reg[0x610/4]|=8;
  assert(mt8167_accdet_gpio_acquire(&s,&io)&&!s.held&&!m.writes);
 }
 m=initial();memset(&s,0,sizeof(s));m.reg[0x510/4]|=8;m.reg[0x610/4]|=8;
 assert(!mt8167_accdet_gpio_acquire(&s,&io));assert(!mt8167_accdet_gpio_release(&s,&io,1));assert((m.reg[0x510/4]&8)&&(m.reg[0x610/4]&8));
 for(unsigned failure=0;failure<5;failure++){
  m=initial();memset(&s,0,sizeof(s));assert(!mt8167_accdet_gpio_acquire(&s,&io));unsigned writes=m.writes;
  if(failure==0)m.reg[0x330/4]|=0x1000;if(failure==1)m.reg[0x510/4]|=8;if(failure==2)m.fail_read=1;if(failure==3)m.drop_write=1;
  assert(mt8167_accdet_gpio_release(&s,&io,failure!=4)&&s.held&&s.poisoned);
  assert(m.writes==writes+(failure==3)); /* surprise: zero post-removal reads/writes */
 }
 for(unsigned failure=0;failure<3;failure++){
  m=initial();memset(&s,0,sizeof(s));m.fail_write=failure==0;m.drop_write=failure==1;m.fail_read=failure==2;
  assert(mt8167_accdet_gpio_acquire(&s,&io));assert(failure==2?!s.held:s.held&&s.poisoned);
 }
 puts("GPIO19 stock/legacy bias, real lease+socket, D0 reacquire, shared IES restore, foreign pin refusal and failure/surprise retain PASS (MMIO model only)");
}
