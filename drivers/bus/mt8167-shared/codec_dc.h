/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_CODEC_DC_H
#define A04_MT8167_CODEC_DC_H
#include <stdint.h>
/* ABB_AFE_CON3/11, Linux mt8167-codec-utils.c. Caller owns AFE, holds
 * the audio lease and proves the downstream Class D remains muted. Values
 * are signed 16-bit DC compensation codes, not millivolts. Linux uses a
 * fixed 0x4b4b for the initial DAC stage and measured values thereafter. */
struct a04_codec_dc_io {
 void *context;
 int (*read)(void *,unsigned,uint32_t *);
 int (*write)(void *,unsigned,uint32_t);
 int (*delay_us)(void *,unsigned);
};
struct a04_codec_dc { int poisoned; };
static int a04_codec_dc_set(struct a04_codec_dc *s,const struct a04_codec_dc_io *io,int16_t code)
{
 uint32_t v,control;
 if(!s || !io || !io->read || !io->write || s->poisoned)return -1;
 if(io->read(io->context,0xf0cu,&v) || io->read(io->context,0xf2cu,&control))return -1;
 /* Set only the left-channel coefficient, preserving the other bits. */
 if(io->write(io->context,0xf0cu,(v&~0xffffu)|(uint16_t)code) ||
    io->write(io->context,0xf2cu,(control&~0x200u)|((control&2u)?0u:0x200u))) {
  s->poisoned=1;return -1;
 }
 return 0;
}
static int a04_codec_dc_ramp(struct a04_codec_dc *s,const struct a04_codec_dc_io *io,
                             int16_t from,int16_t to,unsigned cap_index)
{
 int32_t current=from,target=to,step;unsigned delay;
 if(!s || !io || !io->delay_us || s->poisoned || cap_index>3)return -1;
 step=cap_index<2?0x787:0x181;delay=cap_index<2?2000u:400u;
 /* Match Linux: even equal endpoints are latched once. */
 do {
  if(current<target) {current+=step;if(current>target)current=target;}
  else if(current>target) {current-=step;if(current<target)current=target;}
  if(a04_codec_dc_set(s,io,(int16_t)current)){s->poisoned=1;return -1;}
  if(io->delay_us(io->context,delay)){s->poisoned=1;return -1;}
 } while(current!=target);
 return 0;
}
#endif
