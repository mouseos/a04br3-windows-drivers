/* SPDX-License-Identifier: GPL-2.0-only
 * MT8167 Pass1 RAW10 configuration port, a04br3-linux-drivers 3d2c74e.
 * This is the source baseline, NOT a fix for its unresolved NCSI2->MUX
 * no-frame boundary. Do not import MT2712 CAMSV offsets or failed tunings.
 * Register semantics: CT3 7c478212 cameraisp/src/mt8167/inc/camera_isp.h.
 * No shared TOP/GPIO/PMIC writes, no M4U/security reprogramming. */
#include "capture.h"
#define W(off,val) o->write(o->context,0,off,val)
#define R(off) o->read(o->context,0,off)
static void update(const struct a04_hi556_receiver_io *o,unsigned r,unsigned at,uint32_t mask,uint32_t value)
{o->write(o->context,r,at,(o->read(o->context,r,at)&~mask)|(value&mask));}
static int reset(const struct a04_hi556_receiver_io *o)
{
 W(0x414,R(0x414)&~1u);W(0x410,R(0x410)&~1u);W(0x20,0);W(0x0c,0);
 W(0x5c,1);
 unsigned i;for(i=0;i<1000;i++){
  if(R(0x5c)&2u)break;
  if(o->delay_us(o->context,1))return A04_CAM_DELAY;
 }
 if(i==1000)return A04_CAM_RESET_TIMEOUT;
 W(0x200,0x003f01fe);W(0x5c,5);
 if(o->delay_us(o->context,1))return A04_CAM_DELAY;
 W(0x5c,4);W(0x5c,0);
 return (R(0x5c)&5u)||(R(0x0c)&1u)||(R(0x414)&1u)||(R(0x410)&1u)?A04_CAM_READBACK:0;
}
int a04_hi556_receiver_gate(const struct a04_hi556_receiver_io *o,struct a04_hi556_capture_state *s)
{
 if(!o||!s||!o->read||!o->write||!o->delay_us||!o->live){if(s)s->uncertain=1;return A04_CAM_ARGUMENT;}
 if(o->live(o->context)!=1){s->uncertain=1;return A04_CAM_NOT_LIVE;}
 /* Disable new take-picture requests before snapshotting SOF. If a second
  * frame started before stop, a direct single-buffer image is ambiguous. */
 W(0x414,R(0x414)&~1u);W(0x410,R(0x410)&~1u);
 if((R(0x414)|R(0x410))&1u){s->uncertain=1;return A04_CAM_READBACK;}
 s->sof_end=R(0x430);s->gated=1;return 0;
}
int a04_hi556_receiver_stop(const struct a04_hi556_receiver_io *o,struct a04_hi556_capture_state *s)
{
 if(!o||!s||!o->read||!o->write||!o->delay_us||!o->live)return A04_CAM_ARGUMENT;
 if(o->live(o->context)!=1){s->uncertain=1;return A04_CAM_NOT_LIVE;}
 if(!s->gated){int e=a04_hi556_receiver_gate(o,s);if(e)return e;}
 {int e=reset(o);if(e){s->uncertain=1;return e;}}
 s->armed=0;s->quiesced=1;return 0;
}
int a04_hi556_receiver_arm(const struct a04_hi556_receiver_io *o,struct a04_hi556_capture_state *s,uint64_t dma,unsigned bytes)
{
 if(!o||!s||!o->read||!o->write||!o->delay_us||!o->live||s->armed||s->uncertain||
    !dma||(dma&63u)||dma>UINT32_MAX||bytes!=A04_HI556_FRAME_BYTES||bytes-1>UINT32_MAX-dma)return A04_CAM_ARGUMENT;
 if(o->live(o->context)!=1)return A04_CAM_NOT_LIVE;
 s->stage=1;
 /* Other LARB1 ports belong to other clients. Require physical/bypass and
  * non-secure IMGO already established; never clear a shared M4U bit here. */
 if((o->read(o->context,2,0xfc0)|o->read(o->context,2,0xfc4))&1u)return A04_CAM_MMU_UNASSIGNED;
 {int e=reset(o);if(e){s->uncertain=1;return e;}}s->quiesced=1;s->stage=2;
 /* The resource has ISP Pass1 at offset0, SENINF at4000, not CAMSV+9000. */
 W(0x04,0x40001001);W(0x150,0x0001ffff);W(0x3dc,0);
 W(0x18,0x00010050);W(0x74,0x00d8008c);W(0x78,0x70180100);W(0x7c,0x000634f7);
 W(0x10,0x00011006);W(0x1c,1);W(0x20,0x80000000); /* polling: keep IRQ sources masked */
 W(0x418,A04_HI556_WIDTH<<16);W(0x41c,A04_HI556_HEIGHT<<16);W(0x420,0);W(0x410,4);W(0x414,0x1000);
 W(0x310,A04_HI556_STRIDE);W(0x308,(A04_HI556_WIDTH*10/8)-1);W(0x30c,A04_HI556_HEIGHT-1);
 W(0x314,0x08100850);W(0x318,0x00100800);W(0x31c,0);W(0x304,0);W(0xe00,0);
 W(0x300,(uint32_t)dma);
 /* Analog receiver enable from the source clock-enable path, no power-cycle. */
 static const unsigned enables[]={0x800,0x804,0x808,0x80c,0x810,0x820,0x824};
 for(unsigned i=0;i<7;i++)update(o,1,enables[i],1,1);
 /* Analog operations confined to MIPI RX camera resource. */
 o->write(o->context,1,0x848,0);update(o,1,0x84c,0x01041041,0);update(o,1,0x850,1,0);
 static const unsigned lanes[]={0x800,0x804,0x808,0x80c,0x810};
 for(unsigned i=0;i<5;i++)update(o,1,lanes[i],8,8);
 update(o,1,0x800,0x10,0);update(o,1,0x80c,0x10,0);update(o,1,0x810,0x10,0);
 update(o,1,0x824,1,1);if(o->delay_us(o->context,30))return A04_CAM_DELAY;
 update(o,1,0x820,15,3);if(o->delay_us(o->context,1))return A04_CAM_DELAY;
 W(0x4100,0x8001);
 update(o,0,0x43d8,31,31);W(0x4338,1);W(0x433c,0x1541);W(0x4338,5);
 if(o->delay_us(o->context,500))return A04_CAM_DELAY;
 W(0x4338,4);W(0x433c,0);update(o,0,0x43d8,31,0);
 W(0x43b8,0x11);W(0x43ac,0);W(0x43a0,0x05896173);W(0x43a8,0x1400);
 W(0x43b4,0x7fff);W(0x43b0,0); /* no NCSI2 interrupt handler */
 update(o,0,0x4324,0xff000000,0xe4000000);update(o,0,0x43b8,255,0x45);update(o,0,0x43d8,0x10,0);
 W(0x43e4,0);W(0x43dc,0);W(0x43b8,0x12);
 update(o,0,0x4000,0x10c00,0xc00);W(0x4004,0);W(0x4008,0);
 W(0x4120,0x9eff8600);W(0x4120,0x9eff8603);
 if(o->delay_us(o->context,1))return A04_CAM_DELAY;
 W(0x4120,0x9eff8600);
 /* Clear only status observed before arming. No stale done accepted. */
 W(0x24,R(0x24));W(0x28,R(0x28));
 if((R(0x24)&(1u<<10))||(R(0x28)&1u))return A04_CAM_STALE_STATUS;
 s->pass1=s->imgo=0;s->sof_start=R(0x430);s->stage=3;s->quiesced=0;s->armed=1;s->gated=0;
 /* Source single available IMGO buffer, no software requeue while owned. */
 W(0xf4,(R(0xf4)&~0x000f4000u)|0x00010000u);
 if(o->delay_us(o->context,10))return A04_CAM_DELAY;
 W(0xf4,R(0xf4)|0x4000u);
 W(0x0c,1);W(0x410,5);W(0x414,0x1001);
 return 0;
}
int a04_hi556_receiver_poll(const struct a04_hi556_receiver_io *o,struct a04_hi556_capture_state *s)
{
 if(!o||!s||!s->armed||s->uncertain)return A04_CAM_ARGUMENT;
 if(o->live(o->context)!=1)return A04_CAM_NOT_LIVE;
 uint32_t status=R(0x24),dma=R(0x28);
 s->int_status|=status;s->dma_status|=dma;s->imgo_error|=R(0x3c4);
 s->ncsi_status=R(0x43b4);s->mux_status=R(0x4128);s->mux_count=R(0x4154);s->tg_count=R(0x444);
 if(status&(1u<<10))s->pass1=1;
 if(dma&1u)s->imgo=1;
 /* TG/queue/IMGO/DMA errors are never covered by a coincident DONE. */
 if((status&((1u<<4)|(1u<<13)|(1u<<20)|(1u<<30)))||(dma&((1u<<8)|(1u<<12)))||s->imgo_error)return A04_CAM_HARDWARE_ERROR;
 return s->pass1&&s->imgo?1:0;
}
