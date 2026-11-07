/* SPDX-License-Identifier: GPL-2.0-only
 * Linux MT8167 suspend: wait AHB idle, pause, disable periodic sensing.
 * Add verified source masking and external IRQ drain before that sequence. */
#include "stop.h"
#define T 0x1100d000u
static int read_reg(const struct a04_thermal_hw *h,unsigned offset,uint32_t *v)
{ return !h->qualified(h->ctx)||h->read32(h->ctx,T+offset,v)||*v==UINT32_MAX?-1:0; }
static int write_reg(const struct a04_thermal_hw *h,unsigned offset,uint32_t v)
{ return !h->qualified(h->ctx)||h->write32(h->ctx,T+offset,v)?-1:0; }
static int ahb_idle(const struct a04_thermal_hw *h)
{
 unsigned n;uint32_t v;
 for(n=0;n<50;n++) {
  if(read_reg(h,0x418,&v))return -1;
  if(!(v>>16))return 0;
  if(n!=49&&(!h->qualified(h->ctx)||h->delay_us(h->ctx,2)))return -1;
 }
 return -1;
}
int a04_thermal_stop(struct a04_thermal_stop_state *s,const struct a04_thermal_stop_io *io)
{
 const struct a04_thermal_hw *h;uint32_t v;
 if(!s||!io||!(h=io->hw)||!h->qualified||!h->read32||!h->write32||!h->delay_us||
    !io->drain_irq||s->touched||s->quarantined||!h->qualified(h->ctx))return -1;
 s->touched=1;
 if(write_reg(h,0x00c,0)||read_reg(h,0x00c,&v)||v)goto retain;
 s->irq_masked=1;
 /* Do not guess TEMPMONINTSTS clear semantics from the Linux ISR's reads. */
 if(io->drain_irq(io->context)||!h->qualified(h->ctx))goto retain;
 s->irq_drained=1;
 if(ahb_idle(h)||read_reg(h,0x03c,&v)||write_reg(h,0x03c,v|0x10eu)||
    read_reg(h,0x03c,&v)||(v&0x10eu)!=0x10eu)goto retain;
 if(write_reg(h,0,0)||read_reg(h,0,&v)||v)goto retain;
 s->periodic_off=1;
 if(ahb_idle(h))goto retain;
 s->ahb_idle=1;
 /* Check masking again at the final boundary; external ownership must still
  * exclude a firmware or OS writer. This is not proof of firmware exclusion. */
 if(read_reg(h,0x00c,&v)||v||!h->qualified(h->ctx))goto retain;
 return 0;
retain:
 s->quarantined=1;return -1;
}
