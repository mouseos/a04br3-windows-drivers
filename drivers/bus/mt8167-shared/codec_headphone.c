/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "codec_headphone.h"
#include "codec_calibration.h"
/* 0=parent APMIXED mapping, 1=child-bound AFE mapping. No MMIO acquisition. */
static const unsigned offsets[5]={0x704,0x708,0xf00,0xf0c,0xf10};
static const unsigned banks[5]={0,0,1,1,1};
static const uint32_t masks[5]={0x1fu<<6|0x3fu|3u<<11|3u<<14,
 (1u<<18)|(3u<<19)|(1u<<21)|(1u<<22)|(3u<<23)|(1u<<25)|(1u<<27)|(1u<<28),1u,0xffffu,0xffffu};
static int qualified(struct a04_hp_route *s,const struct a04_hp_io *io)
{return io->qualified(io->context,s->calibration.generation)==0;}
static int update(struct a04_hp_route *s,const struct a04_hp_io *io,unsigned n,uint32_t mask,uint32_t target)
{
 uint32_t v,after;
 if(!qualified(s,io)||io->read(io->context,banks[n],offsets[n],&v)||
    (v&masks[n])!=s->expected[n]||(target&~mask))return -1;
 if(io->write(io->context,banks[n],offsets[n],(v&~mask)|target)||
    io->read(io->context,banks[n],offsets[n],&after)||
    (after&masks[n])!=((s->expected[n]&~mask)|target))return -1;
 s->expected[n]=(s->expected[n]&~mask)|target;return 0;
}
static int latch(struct a04_hp_route *s,const struct a04_hp_io *io,int16_t left,int16_t right)
{
 uint32_t control,after,target;
 if(update(s,io,3,0xffffu,(uint16_t)left)||update(s,io,4,0xffffu,(uint16_t)right)||
    !qualified(s,io)||io->read(io->context,1,0xf2c,&control))return -1;
 target=(control&2u)?0u:0x200u;
 if(io->write(io->context,1,0xf2c,(control&~0x200u)|target)||
    io->read(io->context,1,0xf2c,&after)||(after&0x200u)!=target)return -1;
 return 0; /* Latch requested, not a fabricated hardware drain/ADC result. */
}
static int delay(struct a04_hp_route *s,const struct a04_hp_io *io,unsigned us)
{return !qualified(s,io)||io->delay_us(io->context,us);}
static int32_t step(int32_t value,int32_t target,unsigned delta)
{
 if(value<target){value+=(int32_t)delta;if(value>target)value=target;}
 else if(value>target){value-=(int32_t)delta;if(value<target)value=target;}
 return value;
}
static int ramp(struct a04_hp_route *s,const struct a04_hp_io *io,
 const int16_t from[2],const int16_t to[2],int op_stage)
{
 static const unsigned op_steps[8]={4,3,2,2,2,1,1,1};
 int32_t l=from[0],r=from[1];unsigned i,dl,dr,us;
 unsigned cap=s->calibration.cap_index;
 dl=op_stage?op_steps[s->calibration.gain[0]]:cap<2?0x787u:0x181u;
 dr=op_stage?op_steps[s->calibration.gain[1]]:cap<2?0x787u:0x181u;
 us=op_stage?(cap<2?75u:125u):(cap<2?2000u:400u);
 /* Signed16 endpoints bound the walk. Callback enforces the owner's deadline. */
 for(i=0;i<65536;i++){
  l=step(l,to[0],dl);r=step(r,to[1],dr);
  if(latch(s,io,(int16_t)l,(int16_t)r)||delay(s,io,us))return -1;
  if(l==to[0]&&r==to[1])return 0;
 }
 return -1;
}
static int valid_io(const struct a04_hp_io *io)
{return io&&io->qualified&&io->read&&io->write&&io->delay_us&&io->verify_dma_irq_drained;}
static int start_route(struct a04_hp_route *s,const struct a04_hp_io *io,const struct a04_hp_calibration *c,int release_output)
{
 uint32_t con1,con2,afe;unsigned i;
 if(!s||!valid_io(io)||!c||s->held||s->poisoned||!c->generation||c->measured_channels!=3||
    c->cap_index>3||c->gain[0]>7||c->gain[1]>7||io->qualified(io->context,c->generation))return -1;
 /* Common-domain supplies belong to the parent. Reject the existing voice,
  * DAC or HP output route rather than sharing its register/DMA ownership. */
 if(io->read(io->context,0,0x704,&con1)||io->read(io->context,0,0x708,&con2)||
    io->read(io->context,1,0xf00,&afe)||
    (con1&((3u<<14)|(3u<<11)))||(con2&(1u<<8))||(afe&1u)||
    !(con1&(1u<<13))||(con2&((1u<<16)|(1u<<17)|(1u<<27)))!=((1u<<16)|(1u<<17)|(1u<<27)))return -1;
 for(i=0;i<5;i++){
  uint32_t v;if(io->read(io->context,banks[i],offsets[i],&v))return -1;
  s->saved[i]=s->expected[i]=v&masks[i];
 }
 s->calibration=*c;s->held=1;
 /* hp_depop_setup: bias0, charge/cap, 1.35V, VCM gen, series62.5ohm,
  * temporarily gate DAC clock and hold the depop mux. All masks are owned. */
 if(update(s,io,0,0x1fu<<6,0)||
    update(s,io,1,1u<<18,1u<<18)||update(s,io,1,3u<<19,c->cap_index<<19)||
    update(s,io,1,1u<<21,0)||update(s,io,1,1u<<22,1u<<22)||
    update(s,io,1,3u<<23,3u<<23)||update(s,io,1,1u<<27,0)||
    update(s,io,1,1u<<25,1u<<25)||update(s,io,1,1u<<28,0)||
    update(s,io,1,1u<<27,1u<<27)||update(s,io,2,1u,1u)||
    latch(s,io,c->vcm[0],c->vcm[1])||update(s,io,0,3u<<14,3u<<14)||
    ramp(s,io,c->vcm,c->dac,0)||update(s,io,0,0x3fu,0)||
    update(s,io,0,3u<<11,3u<<11)||
    update(s,io,0,0x3fu,c->gain[0]|(c->gain[1]<<3))||
    update(s,io,1,1u<<28,1u<<28)||ramp(s,io,c->dac,c->op,1)||
    (release_output&&(update(s,io,1,1u<<25,0)||update(s,io,1,1u<<22,0)))){
  s->poisoned=1;return -2;
 }
 s->active=1;return 0;
}
int a04_codec_headphone_start(struct a04_hp_route*s,const struct a04_hp_io*io,const struct a04_hp_calibration*c){return start_route(s,io,c,1);}
int a04_codec_headphone_start_muted(struct a04_hp_route*s,const struct a04_hp_io*io,const struct a04_hp_calibration*c){return start_route(s,io,c,0);}
int a04_codec_headphone_stop(struct a04_hp_route *s,const struct a04_hp_io *io)
{
 unsigned i;
 if(!s||!valid_io(io)||!s->held||!s->active||s->poisoned||!qualified(s,io)||
    io->verify_dma_irq_drained(io->context))return -1;
 if(update(s,io,1,1u<<22,1u<<22)||update(s,io,1,1u<<25,1u<<25)||
    ramp(s,io,s->calibration.op,s->calibration.dac,1)||update(s,io,1,1u<<28,0)||
    update(s,io,0,0x3fu,0)||update(s,io,0,3u<<11,0)||
    ramp(s,io,s->calibration.dac,s->calibration.vcm,0)||
    update(s,io,0,3u<<14,0)||update(s,io,2,1u,0)||
    latch(s,io,(int16_t)s->saved[3],(int16_t)s->saved[4]))goto fault;
 /* Restore only owned masks, preserving live uplink/common domain fields.
  * DC control is a handshake, restored values are re-latched, never raw-reset. */
 for(i=2;i>0;i--)if(update(s,io,i-1,masks[i-1],s->saved[i-1]))goto fault;
 s->active=0;s->held=0;return 0;
fault:s->poisoned=1;return -2;
}

int a04_codec_headphone_calibration(const struct a04_hp_measurements *m,struct a04_hp_calibration *out)
{
 struct a04_hp_calibration c;unsigned channel,i;
 if(!m||!out||!m->generation||m->measured_channels!=3||m->cap_index>3||m->gain[0]>7||m->gain[1]>7)return -1;
 c.generation=m->generation;c.measured_channels=3;c.cap_index=m->cap_index;
 for(channel=0;channel<2;channel++){
  for(i=0;i<3;i++)if(m->uv[channel][i]<0||m->uv[channel][i]>1500000)return -1;
  c.gain[channel]=m->gain[channel];
  /* CT3 gather_comp_val uses this documented DAC-stage coefficient only
   * after actual A/B/C acquisition; zero-initialized profiles are rejected. */
  c.vcm[channel]=0x4b4b;
  if(a04_codec_calculate_comp(m->uv[channel][1],m->uv[channel][2],m->gain[channel],&c.dac[channel])||
     a04_codec_calculate_comp(m->uv[channel][0],m->uv[channel][2],m->gain[channel],&c.op[channel]))return -1;
 }
 *out=c;return 0;
}

/* CT3 hp_depop_enable/disable clamp, without dismantling DAC, clocks or DMA.
 * Retained STOP is not a fabric-drain or resource-release certificate. */
int a04_codec_headphone_mute_retained(struct a04_hp_route *s,const struct a04_hp_io *io)
{
 if(!s||!valid_io(io)||!s->held||!s->active||s->poisoned)return -1;
 if(update(s,io,1,1u<<22,1u<<22)||update(s,io,1,1u<<25,1u<<25)){s->poisoned=1;return -2;}
 return 0;
}
int a04_codec_headphone_resume_retained(struct a04_hp_route *s,const struct a04_hp_io *io)
{
 if(!s||!valid_io(io)||!s->held||!s->active||s->poisoned)return -1;
 if(update(s,io,1,1u<<25,0)||update(s,io,1,1u<<22,0)){s->poisoned=1;return -2;}
 return 0;
}
