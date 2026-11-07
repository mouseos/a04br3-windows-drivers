/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Register facts independently implemented from CT3 GPL 7c478212:
 * mt8167-afe-pcm.c memif_data, dais_hw_params, dais_trigger, enable/disable_irq.
 * No GPL implementation text, SRAM/tone transport or fabricated position. */
#include "hfp_memif.h"
static const unsigned regs[11]={0x10,0x14,0x40,0x48,0x90,0x98,0x3d8,0x3a0,0x3ac,0x3b0,0x3b4};
static const uint32_t masks[11]={0x03000013,0x0020000f,~0u,~0u,~0u,~0u,0x03030000,0xff3,0x3ffff,0x3ffff,3};
static int read(struct mt8167_hfp_memif *m,unsigned off,uint32_t *v)
{return m->io.read(m->io.context,off,v);}
static int write(struct mt8167_hfp_memif *m,unsigned off,uint32_t v)
{return m->io.write(m->io.context,off,v);}
static int update(struct mt8167_hfp_memif *m,unsigned off,uint32_t mask,uint32_t bits)
{uint32_t v;if(read(m,off,&v)||write(m,off,(v&~mask)|(bits&mask))||read(m,off,&v)||(v&mask)!=(bits&mask)){m->fault=1;return -1;}return 0;}
static int valid(const struct mt8167_hfp_ring *r)
{return r&&r->base&&r->generation&&r->bytes&&r->bytes<=65536&&!(r->base&4095)&&!(r->bytes&4095)&&r->base<=0xffffffffu-(r->bytes-1);}
static int program(struct mt8167_hfp_memif *m,unsigned d,const struct mt8167_hfp_ring *r)
{unsigned base=d?0x90:0x40,end=d?0x98:0x48;
 if(!valid(r)||write(m,base,r->base)||write(m,end,r->base+r->bytes-1)){m->fault=1;return -1;}
 uint32_t v;if(read(m,base,&v)||v!=r->base||read(m,end,&v)||v!=r->base+r->bytes-1){m->fault=1;return -1;}
 m->current[d]=*r;return 0;}
int mt8167_hfp_memif_prepare(struct mt8167_hfp_memif *m,const struct mt8167_hfp_io *io,const struct mt8167_hfp_ring fallback[2])
{
 uint32_t v;unsigned i;
 if(!m||!io||!io->read||!io->write||m->prepared||m->exposed||m->fault||!fallback||!valid(&fallback[0])||!valid(&fallback[1]))return -1;
 m->io=*io;
 /* Full AFE owner, no memif or IRQ source may already be running. */
 if(read(m,0x10,&v)||(v&0xfe)||read(m,0x3a0,&v)||(v&0x4003))return -1;
 for(i=0;i<11;i++)if(read(m,regs[i],&m->saved[i]))return -1;
 m->prepared=1;m->fallback[0]=fallback[0];m->fallback[1]=fallback[1];
 if(program(m,0,&fallback[0])||program(m,1,&fallback[1]))goto fault;
 /* DL1 mono bit21 and 8k fs=0; DAI 8k bits25:24=0, 16bit formats=0.
  * Bit31 selectors are not guessed; 32-bit base/end are validated by the DMA adapter; no guessed MSB fields. */
 if(update(m,0x14,0x0020000f,1u<<21)||update(m,0x10,0x03000000,0)||
    update(m,0x3d8,0x03030000,0)||
    update(m,0x3a0,0xff3,0)||update(m,0x3ac,0x3ffff,128)||
    update(m,0x3b0,0x3ffff,128)||update(m,0x3b4,3,3)||write(m,0x3a8,3))goto fault;
 return 0;
 fault:m->fault=1;return -1;
}
int mt8167_hfp_memif_start(struct mt8167_hfp_memif *m)
{
 if(!m||!m->prepared||m->running||m->fault)return -1;
 /* Publish sticky exposure BEFORE any DMA enable. */
 m->exposed=1;
 if(update(m,0x10,0x13,0x13)||update(m,0x3a0,3,3)){m->fault=1;return -1;}
 m->running=1;return 0;
}
int mt8167_hfp_memif_bind(struct mt8167_hfp_memif *m,unsigned d,const struct mt8167_hfp_ring *r)
{
 uint32_t bit=d?(1u<<4):(1u<<1);
 if(!m||d>1||!valid(r)||!m->running||m->fault||(m->active&(1u<<d)))return -1;
 /* Fallback remains pinned, so no false certificate for its old accesses. */
 if(update(m,0x10,bit,0)||program(m,d,r)||update(m,0x10,bit,bit))return -1;
 m->client[d]=*r;m->active|=1u<<d;return 0;
}
int mt8167_hfp_memif_inactive(struct mt8167_hfp_memif *m,unsigned d)
{
 uint32_t bit=d?(1u<<4):(1u<<1);
 if(!m||d>1||!m->running||m->fault)return -1;
 if(!(m->active&(1u<<d)))return 0;
 if(update(m,0x10,bit,0)||program(m,d,&m->fallback[d])||update(m,0x10,bit,bit))return -1;
 m->active&=~(1u<<d);return 0;
}
int mt8167_hfp_memif_position(struct mt8167_hfp_memif *m,unsigned d,uint32_t generation,uint32_t *offset)
{
 uint32_t cur;const struct mt8167_hfp_ring *r;
 if(!m||d>1||!offset||!m->running||m->fault||!(m->active&(1u<<d)))return -1;
 r=&m->current[d];if(generation!=r->generation||read(m,d?0x9c:0x44,&cur)||cur<r->base||cur>=r->base+r->bytes||((cur-r->base)&1))return -1;
 *offset=cur-r->base;return 0;
}
int mt8167_hfp_memif_irq(struct mt8167_hfp_memif *m,uint32_t *serviced)
{
 uint32_t status;if(!m||!serviced||!m->prepared||m->fault||read(m,0x3a4,&status))return -1;
 *serviced=status&3;if(!*serviced)return 0;
 if(write(m,0x3a8,*serviced)){m->fault=1;return -1;}return 0;
}
int mt8167_hfp_memif_stop_registers(struct mt8167_hfp_memif *m)
{
 if(!m||!m->prepared)return -1;
 if(update(m,0x10,0x12,0)||update(m,0x3a0,3,0)||update(m,0x3b4,3,0)||write(m,0x3a8,3))return -1;
 m->running=0;m->active=0;return 0;
}
int mt8167_hfp_memif_restore(struct mt8167_hfp_memif *m)
{
 unsigned i;if(!m||!m->prepared||m->running||m->exposed||m->fault)return -1;
 for(i=11;i;i--)if(update(m,regs[i-1],masks[i-1],m->saved[i-1]))return -1;
 m->prepared=0;return 0;
}
