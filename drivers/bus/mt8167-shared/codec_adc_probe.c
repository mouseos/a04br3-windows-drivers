/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "codec_adc_probe.h"
#include "codec_calibration.h"
#define ADC_CON1 0x004u
#define ADC_CON2 0x010u
#define ADC_DAT0 0x014u
#define ADC_READY 0x1000u
static int wait_bit(const struct a04_codec_adc_io *io,uint32_t reg,uint16_t mask,int set)
{
 unsigned i;uint16_t v;
 for(i=0;i<31;i++) {
  if(io->read16(io->context,reg,&v))return -1;
  if(!!(v&mask)==set)return 0;
  if(io->delay_us(io->context,1000))return -1;
 }
 return -1;
}
int a04_codec_adc_capture(struct a04_codec_adc_probe *state,const struct a04_codec_adc_io *io,unsigned channel,struct a04_codec_adc_samples *out)
{
 struct a04_codec_adc_samples tmp;int32_t uv[22];uint16_t con,data,bit;unsigned i;
 if(!state || state->poisoned || !io || !out || (channel!=7 && channel!=8) ||
    !io->qualified || !io->read16 || !io->write16 || !io->delay_us || !io->qualified(io->context))return -1;
 bit=(uint16_t)(1u<<channel);
 /* Existing trigger means another conversion or unknown inherited state. */
 if(io->read16(io->context,ADC_CON1,&con) || (con&bit))return -1;
 for(i=0;i<22;i++) {
  if(!io->qualified(io->context) || wait_bit(io,ADC_CON2,1,0) ||
     io->read16(io->context,ADC_CON1,&con) || (con&bit) ||
     wait_bit(io,ADC_DAT0+channel*4u,ADC_READY,0) ||
     io->write16(io->context,ADC_CON1,(uint16_t)(con|bit)) ||
     io->delay_us(io->context,25) || wait_bit(io,ADC_DAT0+channel*4u,ADC_READY,1) ||
     io->read16(io->context,ADC_DAT0+channel*4u,&data) ||
     io->read16(io->context,ADC_CON1,&con) || !(con&bit) ||
     io->write16(io->context,ADC_CON1,(uint16_t)(con&~bit)) ||
     wait_bit(io,ADC_DAT0+channel*4u,ADC_READY,0)) { state->poisoned=1;return -1; }
  tmp.raw[i]=(uint16_t)(data&0xfffu);
  if(a04_codec_nominal_hp_uv(tmp.raw[i],&uv[i])) {state->poisoned=1;return -1;}
 }
 if(a04_codec_average_hp_uv(uv,22,&tmp.average_uv)) {state->poisoned=1;return -1;}
 *out=tmp;return 0;
}
