/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Register/protocol facts: CT3 Linux 7c478212 (mt8167 MMC), SD core sd.c,
 * and BSD-licensed A04BR3 SdCardBlockIoDxe / existing msdc PIO contract.
 * Independent SD profile: no changes to the boot eMMC implementation.
 */
#include "sd.h"
#define CFG 0x000u
#define IOCON 0x004u
#define PS 0x008u
#define INT 0x00cu
#define INTEN 0x010u
#define FIFO 0x014u
#define TX 0x018u
#define RX 0x01cu
#define SDCFG 0x030u
#define CMD 0x034u
#define ARG 0x038u
#define STS 0x03cu
#define RESP 0x040u
#define BLK 0x050u
#define EMMC 0x070u
#define DMA 0x09cu
#define CMD_EVENTS 0x700u
#define DATA_EVENTS 0xd000u
#define R1_ERROR 0xfff98008u /* SD R1 errors, including AKE_SEQ_ERROR; no state bits */
struct run {const struct a04_sd_ops *o;uint64_t deadline;};
static uint32_t rd(struct run *r,unsigned off){return r->o->io.read32(r->o->io.context,off);}
static void wr(struct run *r,unsigned off,uint32_t v){r->o->io.write32(r->o->io.context,off,v);}
static enum a04_sd_result check(struct run *r)
{
 if(r->o->live(r->o->io.context)!=1)return SD_REMOVED;
 return r->o->now_100ns(r->o->io.context)>=r->deadline?SD_TIMEOUT:SD_OK;
}
static enum a04_sd_result wait_bits(struct run *r,unsigned off,uint32_t mask,uint32_t value)
{
 for(unsigned n=0;n<2000000u;n++){
  enum a04_sd_result s=check(r);if(s)return s;
  if((rd(r,off)&mask)==value)return SD_OK;
  if(!(n&255u))r->o->delay_us(r->o->io.context,1);
 }return SD_TIMEOUT;
}
static enum a04_sd_result clear_fifo(struct run *r)
{wr(r,FIFO,rd(r,FIFO)|(1u<<31));return wait_bits(r,FIFO,1u<<31,0);}
/* Response encodings match MT8167 MSDC: NONE0, R1/R6/R7=1, R2=2,R3=3,R1b=7. */
static enum a04_sd_result command(struct run *r,unsigned op,uint32_t arg,unsigned response,unsigned bytes,int write,int r1,uint32_t *reply)
{
 enum a04_sd_result s=check(r);if(s)return s;
 s=wait_bits(r,STS,bytes?3u:2u,0);if(s)return s;
 uint32_t pending=rd(r,INT)&CMD_EVENTS;if(pending)wr(r,INT,pending);
 wr(r,ARG,arg);wr(r,CMD,op|(response<<7)|(bytes?(1u<<11)|(bytes<<16):0)|(write?(1u<<13):0));
 for(unsigned n=0;n<2000000u;n++){
  s=check(r);if(s)return s;
  uint32_t bits=rd(r,INT)&CMD_EVENTS;
  if(bits){wr(r,INT,bits);if(bits&0x200)return SD_TIMEOUT;if(bits&0x400)return SD_CRC;
   if(!(bits&0x100))return SD_CARD;
   uint32_t value=rd(r,RESP);if(reply)*reply=value;
   if(r1&&(value&R1_ERROR))return SD_CARD;
   if(response==7)return wait_bits(r,PS,1u<<16,1u<<16);
   return SD_OK;
  }
  if(!(n&255u))r->o->delay_us(r->o->io.context,1);
 }return SD_TIMEOUT;
}
static enum a04_sd_result clock_set(struct run *r,unsigned divisor)
{
 uint32_t cfg=rd(r,CFG);
 /* Source mux/power/pins belong to firmware/platform owner. Only host-local
  * divisor is changed; 181 is the conservative init divisor. Divisor4
  * divides by16: do not advertise a measured source frequency. */
 cfg&=~((0xfffu<<8)|(3u<<20)|(1u<<22)|(1u<<1));cfg|=(divisor<<8)|9u;
 wr(r,CFG,cfg);return wait_bits(r,CFG,1u<<7,1u<<7);
}
static uint32_t bits(const uint32_t words[4],unsigned start,unsigned count)
{
 uint32_t value=0;for(unsigned i=0;i<count;i++){unsigned b=start+i;value|=((words[3u-b/32u]>>(b&31u))&1u)<<i;}return value;
}
static enum a04_sd_result read_ready(struct run *r,const struct a04_sd_card *c)
{
 enum a04_sd_result s=wait_bits(r,PS,1u<<16,1u<<16);if(s)return s;
 for(unsigned i=0;i<200000u;i++){
  uint32_t v=0;s=command(r,13,(uint32_t)c->rca<<16,1,0,0,1,&v);if(s)return s;
  if((v&(1u<<8))&&((v>>9)&15u)==4)return SD_OK;
  if(((v>>9)&15u)!=7&&((v>>9)&15u)!=4)return SD_CARD; /* only programming may legitimately wait */
  r->o->delay_us(r->o->io.context,10);
 }return SD_TIMEOUT;
}
static int valid_ops(const struct a04_sd_ops *o)
{return o&&o->io.read32&&o->io.write32&&o->io.read8&&o->now_100ns&&o->delay_us&&o->live;}
static enum a04_sd_result host_reset(struct run *r)
{
 enum a04_sd_result live=check(r);if(live)return live;
 if(rd(r,DMA)&1u)return SD_UNSUPPORTED; /* Never steal an active DMA owner. */
 wr(r,INTEN,0);wr(r,CFG,rd(r,CFG)|4u|9u);
 enum a04_sd_result s=wait_bits(r,CFG,4u,0);if(s)return s;
 s=clear_fifo(r);if(s)return s;wr(r,INT,rd(r,INT));return SD_OK;
}
enum a04_sd_result a04_sd_initialize(const struct a04_sd_ops *o,struct a04_sd_card *c,uint64_t deadline)
{
 struct run r={o,deadline};enum a04_sd_result s;uint32_t v=0;int version2=0;
 if(!valid_ops(o)||!c)return SD_INVALID;
 struct a04_sd_card fresh={0};*c=fresh;
 s=check(&r);if(s)return s;s=host_reset(&r);if(s)goto fail;
 s=clock_set(&r,181);if(s)goto fail;wr(&r,IOCON,0);wr(&r,EMMC,rd(&r,EMMC)&~(1u<<15));
 wr(&r,SDCFG,(rd(&r,SDCFG)&~(3u<<16))|0xff000000u);
 o->delay_us(o->io.context,1000); /* initial native clocks before CMD0 */
 s=command(&r,0,0,0,0,0,0,NULL);if(s)goto fail;o->delay_us(o->io.context,1000);
 s=command(&r,8,0x1aa,1,0,0,0,&v);
 if(s==SD_OK){if((v&0xfffu)!=0x1aau){s=SD_UNSUPPORTED;goto fail;}version2=1;}
 else if(s!=SD_TIMEOUT)goto fail; /* SD1 optional CMD8 probe only; never a write retry. */
 for(unsigned i=0;i<1000;i++){
  s=command(&r,55,0,1,0,0,1,&v);if(s)goto fail;if(!(v&(1u<<5))){s=SD_UNSUPPORTED;goto fail;}
  s=command(&r,41,0x00300000u|(version2?1u<<30:0),3,0,0,0,&v);if(s)goto fail;
  if(v&(1u<<31))break;
  if(i==999){s=SD_TIMEOUT;goto fail;}o->delay_us(o->io.context,1000);
 }
 if(!(v&0x00300000u)){s=SD_UNSUPPORTED;goto fail;}c->ocr=v;c->high_capacity=!!(v&(1u<<30));
 s=command(&r,2,0,2,0,0,0,NULL);if(s)goto fail;
 for(unsigned i=0;i<4;i++)c->cid[i]=rd(&r,RESP+4u*(3u-i));
 s=command(&r,3,0,1,0,0,0,&v);if(s)goto fail;
 if((v&0xe000u)||!(v>>16)){s=SD_CARD;goto fail;}c->rca=(uint16_t)(v>>16);
 s=command(&r,9,(uint32_t)c->rca<<16,2,0,0,0,NULL);if(s)goto fail;
 for(unsigned i=0;i<4;i++)c->csd[i]=rd(&r,RESP+4u*(3u-i));
 unsigned structure=bits(c->csd,126,2);
 if(structure==1&&c->high_capacity)c->sectors=((uint64_t)bits(c->csd,48,22)+1u)<<10;
 else if(structure==0&&!c->high_capacity){
  unsigned shift=bits(c->csd,47,3)+2u+bits(c->csd,80,4);
  if(shift<9||shift>31){s=SD_UNSUPPORTED;goto fail;}
  c->sectors=((uint64_t)bits(c->csd,62,12)+1u)<<(shift-9u);
 }else {s=SD_UNSUPPORTED;goto fail;} /* SDUC/CSDv3 not supported. */
 if(!c->sectors||c->sectors>0x100000000ULL||(!c->high_capacity&&c->sectors>0x800000u)){s=SD_UNSUPPORTED;goto fail;}
 if(!(bits(c->csd,84,12)&4u)){s=SD_UNSUPPORTED;goto fail;}
 c->write_protected=!!bits(c->csd,12,2)||!(bits(c->csd,84,12)&16u);
 c->mid=(uint8_t)bits(c->cid,120,8);for(unsigned i=0;i<5;i++)c->product[i]=(uint8_t)bits(c->cid,96u-i*8u,8);
 c->revision=(uint8_t)bits(c->cid,56,8);
 s=command(&r,7,(uint32_t)c->rca<<16,7,0,0,1,NULL);if(s)goto fail;
 s=command(&r,16,512,1,0,0,1,NULL);if(s)goto fail;
 s=clock_set(&r,4);if(s)goto fail;s=read_ready(&r,c);if(s)goto fail;
 c->ready=1;return SD_OK;
fail:c->ready=0;c->poisoned=1;return s;
}
enum a04_sd_result a04_sd_transfer(const struct a04_sd_ops *o,struct a04_sd_card *c,uint64_t lba,uint32_t blocks,int write,uint8_t *buffer,uint64_t deadline)
{
 struct run r={o,deadline};enum a04_sd_result s=SD_OK;
 if(!valid_ops(o)||!c||!buffer||!blocks||blocks>A04_SD_MAX_BLOCKS||lba>=c->sectors||blocks>c->sectors-lba)return SD_INVALID;
 if(!c->ready||c->poisoned)return SD_POISONED;
 if(write&&c->write_protected)return SD_PROTECTED;
 for(uint32_t b=0;b<blocks;b++){
  s=check(&r);if(s)goto fail;if(rd(&r,DMA)&1u){s=SD_UNSUPPORTED;goto fail;}
  wr(&r,BLK,1);s=clear_fifo(&r);if(s)goto fail;wr(&r,INT,DATA_EVENTS);
  uint32_t arg=(uint32_t)(lba+b);if(!c->high_capacity)arg<<=9;
  s=command(&r,write?24:17,arg,1,512,write,1,NULL);if(s)goto fail;
  unsigned offset=0;int complete=0;
  for(unsigned n=0;n<4000000u;n++){
   s=check(&r);if(s)goto fail;uint32_t events=rd(&r,INT)&DATA_EVENTS;
   if(events){wr(&r,INT,events);if(events&0x4000){s=SD_TIMEOUT;goto fail;}if(events&0x8000){s=SD_CRC;goto fail;}if(events&0x1000)complete=1;}
   uint32_t fifo=rd(&r,FIFO);unsigned available=write?128u-((fifo>>16)&255u):(fifo&255u);
   if(available>128){s=SD_CARD;goto fail;}
   while(available>=4&&offset<512){
    s=check(&r);if(s)goto fail;uint8_t *p=buffer+(size_t)b*512u+offset;
    if(write){uint32_t v=p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);wr(&r,TX,v);}
    else {uint32_t v=rd(&r,RX);p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
    available-=4;offset+=4;
   }
   if(complete&&offset==512)break;
   if(n==3999999u){s=SD_TIMEOUT;goto fail;}
   if(!(n&255u))o->delay_us(o->io.context,1);
  }
  if(write){s=read_ready(&r,c);if(s)goto fail;}
 }
 return SD_OK;
fail: c->poisoned=1;c->ready=0;return s; /* no partial success or retry */
}
enum a04_sd_result a04_sd_barrier(const struct a04_sd_ops *o,struct a04_sd_card *c,uint64_t deadline)
{
 if(!valid_ops(o)||!c)return SD_INVALID;
 if(!c->ready||c->poisoned)return SD_POISONED;
 struct run r={o,deadline};enum a04_sd_result s=check(&r);if(!s&&(rd(&r,DMA)&1u))s=SD_UNSUPPORTED;if(!s)s=read_ready(&r,c);
 if(s){c->ready=0;c->poisoned=1;}return s;
}
enum a04_sd_result a04_sd_quiesce(const struct a04_sd_ops *o,uint64_t deadline)
{
 if(!valid_ops(o))return SD_INVALID;
 struct run r={o,deadline};return host_reset(&r);
}
