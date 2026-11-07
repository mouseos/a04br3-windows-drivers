/* SPDX-License-Identifier: GPL-2.0-only
 * Linux IMM_auxadc_GetOneChannelValue, CT3 kernel-4.4 fixed 7c478212:
 * busy clear, old ready clear, trigger, 25us, new ready set, 12-bit value.
 * Bounded cleanup is added; no IRQ registration, DMA or thermal AHB program. */
#include "auxadc_pio.h"
#define CH11 0x0800u
#define READY 0x1000u
static int adc_pio_rd(const struct a04_auxadc_pio_io *io,unsigned reg,uint16_t *v)
{ return !io->qualified(io->context) || io->read16(io->context,reg,v) || *v==UINT16_MAX ? -1:0; }
static int adc_pio_wr(const struct a04_auxadc_pio_io *io,unsigned reg,uint16_t v)
{ return !io->qualified(io->context) || io->write16(io->context,reg,v) ? -1:0; }
static int adc_pio_wait(const struct a04_auxadc_pio_io *io,unsigned us)
{ return !io->qualified(io->context) || io->wait_us(io->context,us) ? -1:0; }
static int adc_pio_poll(const struct a04_auxadc_pio_io *io,unsigned reg,uint16_t mask,int set,uint16_t *v)
{
 unsigned n;
 for(n=0;n<32;n++) {
  if(adc_pio_rd(io,reg,v))return -1;
  if(!!(*v&mask)==set)return 0;
  if(n!=31 && adc_pio_wait(io,1000))return -1;
 }
 return -1;
}
enum a04_auxadc_pio_result a04_auxadc_pio_channel11(
 struct a04_auxadc_pio_state *s,const struct a04_auxadc_pio_io *io,uint16_t *raw)
{
 uint16_t control,mode,value=0,trigger,busy;int sampled=0;
 if(!s||!io||!raw||!io->qualified||!io->read16||!io->write16||!io->wait_us||
    s->quarantined||s->touched||!io->qualified(io->context))return A04_PIO_REJECTED;
 /* Do not modify an existing automatic mode/trigger program or another user's
  * active conversion. These observations do not establish firmware exclusion. */
 if(adc_pio_rd(io,0,&mode)||(mode&CH11)||adc_pio_rd(io,4,&control)||control||
    adc_pio_poll(io,0x10,1,0,&busy))return A04_PIO_REJECTED;
 /* A write failure may have reached the ADC. Mark touched before attempting. */
 s->touched=1;
 if(adc_pio_wr(io,4,0))goto retain;
 if(adc_pio_poll(io,0x40,READY,0,&value))goto cleanup;
 if(adc_pio_wr(io,4,CH11))goto retain;
 if(adc_pio_wait(io,25))goto cleanup;
 if(!adc_pio_poll(io,0x40,READY,1,&value))sampled=1;
cleanup:
 /* Local ADC proof only, not IRQ/fabric/firmware drain. Preserve unrelated
  * bits and reject a newly active channel rather than clearing its request. */
 if(adc_pio_rd(io,4,&trigger)||(trigger&~CH11)||adc_pio_wr(io,4,0))goto retain;
 if(adc_pio_poll(io,0x10,1,0,&busy)||adc_pio_rd(io,4,&trigger)||trigger)goto retain;
 s->touched=0;
 if(!sampled || !(value&0xfffu))return A04_PIO_SAMPLE_FAILED_ADC_IDLE;
 *raw=value&0xfffu;return A04_PIO_OK;
retain:
 s->quarantined=1;return A04_PIO_RETAIN;
}
