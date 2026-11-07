/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Register facts: CT3 Linux 7c478212, mt6392-regulator.c, registers.h,
 * clk-mt8167.c and clk-gate.c. No GPL function body copied.
 */
#include "power_owner.h"
#define GATE1 (1u<<18)
#define GATE2 (1u<<29) /* inverted: 1 enables MSDC1_INFRA */
static const uint16_t regs[4]={0x0504,0x0506,0x052a,0x052c};
static const uint16_t masks[4]={0x1000,0x4000,0x0010,0x0080};
static int wait(struct a04_pmic_read*b,unsigned f,uint32_t*v)
{
 for(unsigned n=0;n<b->attempts;n++){
  *v=b->read32(b->context,0xa4);if(((*v>>16)&7)==f)return 0;b->delay(b->context);
 }
 b->poisoned=1;return -1;
}
static int read_pmic(struct a04_pmic_read*b,unsigned i,uint16_t*v)
{
 uint32_t s;
 if(b->poisoned||!(b->read32(b->context,0x9c)&1)||wait(b,0,&s))return -1;
 b->write32(b->context,0xa0,((uint32_t)regs[i]>>1)<<16);
 if(wait(b,6,&s))return -1;
 *v=(uint16_t)s;b->write32(b->context,0xa8,1);return wait(b,0,&s);
}
static int update(struct a04_pmic_read*b,unsigned i,uint16_t bits)
{
 uint16_t v,after,before;uint32_t s;
 if(read_pmic(b,i,&v))return -1;
 before=v;v=(uint16_t)((v&~masks[i])|(bits&masks[i]));
 if(v==before)return 0;
 if(wait(b,0,&s))return -1;
 b->write32(b->context,0xa0,0x80000000u|((uint32_t)regs[i]>>1)<<16|v);
 if(read_pmic(b,i,&after)||((after^v)&masks[i])){b->poisoned=1;return -1;}
 return 0;
}
static int source(struct a04_sd_power*p)
{
 /* Never retune MAINPLL or live mux. Linux MAINPLL: PWR_ON=1, ISO=0,
  * EN bit0, RST_BAR bit27. Readiness is register state, not frequency proof. */
 unsigned h=(p->top_read(p->context,0)>>4)&15;
 if(h!=1 && h!=2 && h!=4 && h!=12)return 0; /* Linux AHB_INFRA parents */
 if(p->held && (h!=p->hclk || (p->pll_read(p->context,0x124)&0x071fffffu)!=p->pll_setting))return 0;
 return ((p->top_read(p->context,0)>>20)&7)==2 &&
  (p->pll_read(p->context,0x130)&3)==1 &&
  (p->pll_read(p->context,0x120)&0x08000001u)==0x08000001u;
}
static int rails(struct a04_sd_power*p)
{
 uint16_t v[4];for(unsigned i=0;i<4;i++)if(read_pmic(p->bus,i,&v[i]))return -1;
 return (v[0]&0x9000)==0x9000 && (v[1]&0xc000)==0xc000 &&
        (v[2]&0x10) && (v[3]&0x80) ? 0:-1;
}
int a04_sd_power_health(struct a04_sd_power*p)
{
 if(!p||!p->held||!p->active||p->poisoned||p->bus->poisoned)return -1;
 if(!source(p)||(p->top_read(p->context,0x24)&GATE1)||
    !(p->top_read(p->context,0x3c)&GATE2)||rails(p)){p->poisoned=1;return -1;}
 return 0;
}
int a04_sd_power_release(struct a04_sd_power*p)
{
 uint16_t v;uint64_t deadline;
 if(!p||!p->held||p->poisoned||p->bus->poisoned)return -1;
 if(!p->active){p->held=0;return 0;} /* Unmodified claim only. */
 deadline=p->now_100ns(p->context)+1000000u; /* fixed total QI budget: 100ms */
 /* Restore only owned fields, never TOP RMW or another consumer's bits.
  * No attempt to repair externally changed live supply/source state. */
 if(a04_sd_power_health(p))return -1;
 p->top_write(p->context,p->gate2?0x6c:0x9c,GATE2);
 p->top_write(p->context,p->gate1?0x54:0x84,GATE1);
 if((p->top_read(p->context,0x24)&GATE1)!=p->gate1 ||
    (p->top_read(p->context,0x3c)&GATE2)!=p->gate2)goto fail;
 /* VQMMC VMC off before VMMC VMCH off; restore selectors only when off.
  * Firmware-on rails retain their original on state, not a forced always-on policy. */
 for(unsigned i=0;i<2;i++)if(!(p->saved[i]&masks[i])&&update(p->bus,i,0))goto fail;
 for(unsigned i=0;i<2;i++)if(!(p->saved[i]&masks[i])){
  unsigned n;
  for(n=0;n<p->bus->attempts;n++){
   if(p->now_100ns(p->context)>=deadline||read_pmic(p->bus,i,&v))goto fail;
   if(!(v&0x8000))break;p->bus->delay(p->bus->context);
  }
  if(n==p->bus->attempts||update(p->bus,i+2,p->saved[i+2]))goto fail;
 }
 p->active=0;p->held=0;return 0;
fail:p->poisoned=1;return -1;
}
int a04_sd_power_acquire(struct a04_sd_power*p)
{
 if(!p||!p->bus||!p->top_read||!p->top_write||!p->pll_read||!p->now_100ns||
    !p->bus->read32||!p->bus->write32||!p->bus->delay||!p->bus->attempts||
    p->held||p->poisoned||p->bus->poisoned||!source(p))return -1;
 for(unsigned i=0;i<4;i++)if(read_pmic(p->bus,i,&p->saved[i]))return -1;
 /* Reject hardware control/foreign-on state and live voltage retuning.
  * Disabled rails may be configured for this fixed non-UHS 3.3 V profile. */
 for(unsigned i=0;i<2;i++){
  if(!(p->saved[i]&masks[i])&&(p->saved[i]&0x8000))return -1;
  if((p->saved[i]&masks[i])&&!(p->saved[i+2]&masks[i+2]))return -1;
 }
 p->gate1=p->top_read(p->context,0x24)&GATE1;
 p->gate2=p->top_read(p->context,0x3c)&GATE2;
 /* Do not ungate a firmware controller before its DMA status is known.
  * This UEFI-ready profile requires both clocks already on for inspection. */
 if(p->gate1 || !p->gate2)return -1;
 p->hclk=(p->top_read(p->context,0)>>4)&15;
 p->pll_setting=p->pll_read(p->context,0x124)&0x071fffffu;
 p->active=0;p->held=1;if(++p->generation==0)++p->generation;
 return 0;
}
int a04_sd_power_enable(struct a04_sd_power*p)
{
 uint64_t deadline;
 /* The fixed kernel consumer checked private MSDC DMA idle while holding
  * the unmodified claim. Its PnP MMIO ownership excludes another SD owner. */
 if(!p||!p->held||p->active||p->poisoned||p->bus->poisoned||!source(p)||
    (p->top_read(p->context,0x24)&GATE1)||!(p->top_read(p->context,0x3c)&GATE2))return -1;
 deadline=p->now_100ns(p->context)+1000000u;
 p->active=1; /* Ambiguous mutation retains lease, no guessed rollback. */
 for(unsigned i=0;i<2;i++)if(!(p->saved[i]&masks[i])&&update(p->bus,i+2,masks[i+2]))goto fail;
 /* Linux MMC_POWER_UP enables VMMC; MMC_POWER_ON enables VQMMC. */
 if(update(p->bus,1,masks[1])||update(p->bus,0,masks[0]))goto fail;
 for(unsigned n=0;n<p->bus->attempts;n++){
  if(p->now_100ns(p->context)>=deadline)goto fail;
  if(!rails(p)){
   p->top_write(p->context,0x84,GATE1);p->top_write(p->context,0x6c,GATE2);
   return a04_sd_power_health(p);
  }
  if(p->bus->poisoned)goto fail;p->bus->delay(p->bus->context);
 }
fail:p->poisoned=1;return -1;
}
