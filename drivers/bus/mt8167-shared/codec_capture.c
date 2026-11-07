/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Register facts: Benesse CT3 kernel-4.4 mt8167-codec.c DAPM CODEC_CLK,
 * UL_CLK, Vcm14, UL_Vref24, AU_MICBIAS0, PGA/ADC, aif_tx, setup_ul_rate.
 * No MT6392 ADC, AUXADC calibration, DMIC wiring or GPIO guesses. */
#include "codec_capture.h"
static int analog(const struct a04_capture_codec_io *i,unsigned r,uint32_t mask,uint32_t target)
{uint32_t v;if(i->analog_read(i->context,r,&v)||i->analog_write(i->context,r,(v&~mask)|(target&mask))||i->analog_read(i->context,r,&v)||(v&mask)!=(target&mask))return -1;return 0;}
static int afe(const struct a04_capture_codec_io *i,unsigned r,uint32_t mask,uint32_t target)
{uint32_t v;if(i->afe_read(i->context,r,&v)||i->afe_write(i->context,r,(v&~mask)|(target&mask))||i->afe_read(i->context,r,&v)||(v&mask)!=(target&mask))return -1;return 0;}
int a04_capture_codec_prepare(struct a04_capture_codec *s,const struct a04_capture_codec_io *i)
{
 uint32_t v,ctrl;unsigned n;static const unsigned ar[3]={0x700,0x704,0x70c},fr[3]={0xf00,0xf04,0xf2c};
 if(!s||!i||!i->analog_read||!i->analog_write||!i->afe_read||!i->afe_write||s->retained||s->poisoned)return -1;
 for(n=0;n<3;n++)if(i->analog_read(i->context,ar[n],&s->saved[n])||i->afe_read(i->context,fr[n],&s->saved[n+3]))return -1;
 if((s->saved[0]&((1u<<24)|(1u<<23)|(1u<<6)|(1u<<5)))||(s->saved[2]&(1u<<28))||(s->saved[3]&3)||
    i->afe_read(i->context,0x10,&v)||(v&0xfe)||i->afe_read(i->context,0x114,&v)||(v&1))return -1;
 s->retained=1;
 /* DAPM supply order, with checked RMW preserving all unrelated fields. */
 if(analog(i,0x704,1u<<20,1u<<20)||analog(i,0x70c,1u<<30,1u<<30)||
    analog(i,0x70c,1u<<21,1u<<21)||
    analog(i,0x700,(1u<<19)|(1u<<1),(1u<<19)|(1u<<1))||
    analog(i,0x700,(1u<<20)|(1u<<2),(1u<<20)|(1u<<2))||
    analog(i,0x70c,1u<<17,1u<<17)||
    analog(i,0x700,(3u<<28)|(3u<<10),0)||
    analog(i,0x700,(1u<<24)|(1u<<6),(1u<<24)|(1u<<6))||
    analog(i,0x700,(1u<<23)|(1u<<5),(1u<<23)|(1u<<5))||
    /* UL codec fs=1 at 48k differs from VUL/IRQ fs=10 and ADDA fs=3. */
    afe(i,0xf04,0xf0,0x10)||i->afe_read(i->context,0xf2c,&ctrl)||
    afe(i,0xf2c,0x100,(ctrl&1)?0:0x100)||
    /* AD_CLK un-gate belongs to parent; TX on does not start VUL DMA. */
    afe(i,0,1u<<24,0)||afe(i,0xf00,2,2)){
  s->poisoned=1;return -2;
 }
 s->prepared=1;return 0;
}
int a04_capture_codec_stop(struct a04_capture_codec *s,const struct a04_capture_codec_io *i)
{
 uint32_t v;
 if(!s||!i||!s->prepared||s->poisoned)return -1;
 if(i->afe_read(i->context,0x10,&v)||(v&8)||i->afe_read(i->context,0x114,&v)||(v&1))return -1;
 if(afe(i,0xf00,2,0)||analog(i,0x700,(1u<<23)|(1u<<5),0)||
    analog(i,0x700,(1u<<24)|(1u<<6),0)||analog(i,0x70c,1u<<17,s->saved[2])||
    analog(i,0x700,(1u<<20)|(1u<<2),s->saved[0])||
    analog(i,0x700,(1u<<19)|(1u<<1),s->saved[0])||
    analog(i,0x70c,1u<<21,s->saved[2])||analog(i,0x70c,1u<<30,s->saved[2])||
    analog(i,0x704,1u<<20,s->saved[1])){s->poisoned=1;return -2;}
 s->prepared=0;return 0;
}
