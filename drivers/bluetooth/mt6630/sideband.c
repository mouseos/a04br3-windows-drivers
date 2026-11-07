/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Independent implementation of protocol/register facts from CT3 kernel
 * 7c4782127d4ea11965be00ce1343056be9bfb769: mt8167-afe-pcm.c set_mrg,
 * enable/disable_mrg, O02/I02/O11 DAPM route; a04br3.dts pin selection.
 * GPL implementation text is not incorporated. No fabricated register values.
 */
#include "sideband.h"
enum {DAIBT=0x1c,MRGIF=0x3c,CONN1=0x24,CONN3=0x2c};
static const unsigned offsets[4]={DAIBT,MRGIF,CONN1,CONN3};
/* Only the independently leased BT fields; preserve every unrelated bit. */
static const uint32_t masks[4]={0x120bu,0xf10001u,0x3060u,0x40u};
static int fault(struct a04_bt_sideband *s)
{s->state=A04_BT_SB_FAULT;return -2;}
static int live(struct a04_bt_sideband *s)
{return s&&s->generation&&s->ops.live(s->ops.context,s->generation)==1;}
static int update(struct a04_bt_sideband *s,unsigned offset,uint32_t mask,uint32_t value)
{
 uint32_t actual;
 if(!live(s)||s->ops.update_afe(s->ops.context,s->generation,offset,mask,value)||
    !live(s)||s->ops.read_afe(s->ops.context,s->generation,offset,&actual)||
    (actual&mask)!=(value&mask))return fault(s);
 return 0;
}
static int delay(struct a04_bt_sideband *s)
{
 if(!live(s)||s->ops.delay_us(s->ops.context,s->generation,100)||!live(s))return fault(s);
 return 0;
}
int a04_bt_sideband_prepare(struct a04_bt_sideband *s,const struct a04_bt_sideband_ops *ops)
{
 unsigned generation=0;int result;
 if(!s||s->state!=A04_BT_SB_OFF||!ops||!ops->acquire||!ops->live||!ops->read_afe||!ops->update_afe||
    !ops->pins||!ops->prepare_dma||!ops->start_dma||!ops->stop_dma_drained||!ops->delay_us||!ops->release_quiescent)return -1;
 s->ops=*ops;
 result=s->ops.acquire(s->ops.context,8000,1,16,&generation);
 if(result==-1)return -1; /* provider guarantees no side effects */
 s->generation=generation;s->state=A04_BT_SB_ACQUIRED;
 if(result||!live(s))return fault(s);
 for(unsigned i=0;i<4;i++){
  if(!live(s)||s->ops.read_afe(s->ops.context,s->generation,offsets[i],&s->saved[i]))return fault(s);
 }
 /* A successful grant must not steal a running FM/MRG/other audio stream. */
 if((s->saved[0]&3)||(s->saved[1]&0x10001u))return fault(s);
 if(!live(s)||s->ops.pins(s->ops.context,s->generation,1)||!live(s)||
    s->ops.prepare_dma(s->ops.context,s->generation,8000,1,16))return fault(s);
 /* Board merge interface: fixed MRG I2S timing field9, DAIBT use-MRG,
  * 8k rate bit9=0, data-ready bit3. Linux hw_params sets both O02 R bits.
  * Select the source-supported I05-only mono DAPM route; capture I02 -> O11 -> DAI memif. */
 if(update(s,MRGIF,0xf00000u,9u<<20)||update(s,DAIBT,0x1208u,0x1008u)||
    update(s,CONN1,0x3060u,0x3020u)||update(s,CONN3,0x40u,0x40u))return -2;
 s->state=A04_BT_SB_PREPARED;return 0;
}
int a04_bt_sideband_start(struct a04_bt_sideband *s)
{
 if(!s||s->state!=A04_BT_SB_PREPARED)return -1;
 /* Stock BE trigger order; never enable the host DMA on a failed route. */
 if(update(s,MRGIF,0x10000u,0x10000u)||update(s,MRGIF,1,1)||delay(s)||update(s,DAIBT,3,3))return -2;
 if(!live(s)||s->ops.start_dma(s->ops.context,s->generation)||!live(s))return fault(s);
 s->state=A04_BT_SB_RUNNING;return 0;
}
int a04_bt_sideband_close(struct a04_bt_sideband *s)
{
 if(!s||(s->state!=A04_BT_SB_PREPARED&&s->state!=A04_BT_SB_RUNNING))return -1;
 /* Even an unstarted prepared DMA mapping requires provider-side rundown.
  * A failed drain retains all ownership; do not stop its clocks or unmap. */
 if(!live(s)||s->ops.stop_dma_drained(s->ops.context,s->generation)||!live(s))return fault(s);
 if(update(s,DAIBT,3,0)||delay(s)||update(s,MRGIF,0x10000u,0)||update(s,MRGIF,1,0))return -2;
 for(unsigned i=0;i<4;i++)if(update(s,offsets[i],masks[i],s->saved[i]&masks[i]))return -2;
 if(!live(s)||s->ops.pins(s->ops.context,s->generation,0)||!live(s)||
    s->ops.release_quiescent(s->ops.context,s->generation))return fault(s);
 s->generation=0;s->state=A04_BT_SB_CLOSED;return 0;
}
