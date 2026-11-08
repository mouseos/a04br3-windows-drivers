/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_ACCDET_GPIO_LEASE_H
#define MT8167_ACCDET_GPIO_LEASE_H
#include <stdint.h>
/* A state object INSIDE the single shared parent, not a GPIO controller,
 * mapping, child owner or arbitrary pin interface. Caller holds parent lock.
 * Stock LK41e008cc: DIR1=03c3, PULLEN1=fc20, PULLSEL1=4020: pin19 is
 * input with both bias bits0. CT3 gpio_request/irq_request owns mode0/IES.
 * Do not infer bias from DIN and do not write PULLEN/PULLSEL or DOUT. */
struct mt8167_accdet_gpio_lease {uint32_t saved[5],expected[5];int held,poisoned;};
struct mt8167_accdet_gpio_io {void*context;int(*read32)(void*,unsigned,uint32_t*);int(*write32)(void*,unsigned,uint32_t);};
static const unsigned mt8167_accdet_gpio_offsets[5]={0x10,0x330,0x910,0x510,0x610};
static const uint32_t mt8167_accdet_gpio_masks[5]={8u,7u<<12,1u<<10,8u,8u};
static int mt8167_accdet_gpio_snapshot(const struct mt8167_accdet_gpio_io*io,uint32_t out[5])
{
 unsigned i;uint32_t v;
 if(!io||!io->read32||!out)return -1;
 for(i=0;i<5;i++){if(io->read32(io->context,mt8167_accdet_gpio_offsets[i],&v)||v==UINT32_MAX)return -1;out[i]=v&mt8167_accdet_gpio_masks[i];}return 0;
}
static int mt8167_accdet_gpio_live(const struct mt8167_accdet_gpio_lease*s,const struct mt8167_accdet_gpio_io*io)
{
 uint32_t v[5];unsigned i;if(!s||!s->held||s->poisoned||mt8167_accdet_gpio_snapshot(io,v))return -1;
 for(i=0;i<5;i++)if(v[i]!=s->expected[i])return -1;return 0;
}
static int mt8167_accdet_gpio_update(struct mt8167_accdet_gpio_lease*s,const struct mt8167_accdet_gpio_io*io,unsigned n,uint32_t target)
{
 uint32_t v,after,mask;
 if(!s||!io||!io->read32||!io->write32||n>2)return -1;mask=mt8167_accdet_gpio_masks[n];
 if((target&~mask)||mt8167_accdet_gpio_live(s,io)||io->read32(io->context,mt8167_accdet_gpio_offsets[n],&v)||v==UINT32_MAX||(v&mask)!=s->expected[n])return -1;
 if((v&mask)!=target){
  if(io->write32(io->context,mt8167_accdet_gpio_offsets[n],(v&~mask)|target)||
     io->read32(io->context,mt8167_accdet_gpio_offsets[n],&after)||after==UINT32_MAX||(after&mask)!=target)return -1;
 }
 s->expected[n]=target;return mt8167_accdet_gpio_live(s,io);
}
static int mt8167_accdet_gpio_acquire(struct mt8167_accdet_gpio_lease*s,const struct mt8167_accdet_gpio_io*io)
{
 uint32_t a[5],b[5];unsigned i;
 if(!s||!io||!io->write32||s->held||s->poisoned||mt8167_accdet_gpio_snapshot(io,a)||mt8167_accdet_gpio_snapshot(io,b))return -1;
 for(i=0;i<5;i++)if(a[i]!=b[i])return -1;
 /* Never commandeer UCTS1/IDDIG/I2S/CLKM/ANT or an output. Stock no-pull
  * is the board's initialized mode; retain legacy verified pull-up too. */
 if(a[0]||a[1]||a[3]!=a[4])return -1;
 for(i=0;i<5;i++)s->saved[i]=s->expected[i]=a[i];s->held=1;
 /* Same Linux acquisition operations, readback checked, other pins preserved.
  * IES bit10 is shared18..20; restore only at whole-parent revoke, never
  * between individual jack reads or while another parent call is admitted. */
 if(mt8167_accdet_gpio_update(s,io,0,0)||mt8167_accdet_gpio_update(s,io,1,0)||mt8167_accdet_gpio_update(s,io,2,1u<<10)){s->poisoned=1;return -1;}
 return 0;
}
static int mt8167_accdet_gpio_release(struct mt8167_accdet_gpio_lease*s,const struct mt8167_accdet_gpio_io*io,int can_access)
{
 int n;if(!s)return -1;if(!s->held)return s->poisoned?-1:0;
 if(!can_access||s->poisoned||mt8167_accdet_gpio_live(s,io)){s->poisoned=1;return -1;}
 for(n=2;n>=0;n--)if(mt8167_accdet_gpio_update(s,io,(unsigned)n,s->saved[n])){s->poisoned=1;return -1;}
 s->held=0;return 0;
}
#endif
