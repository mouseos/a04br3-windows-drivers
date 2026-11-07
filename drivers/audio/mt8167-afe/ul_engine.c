/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Register facts: CT3 kernel-4.4 mt8167-afe-regs.h and mt8167-afe-pcm.c
 * memif_data[VUL], set_adda_in, hw_params, DAPM O09/O10, IRQ2 trigger.
 * Independently implemented; not an AUXADC or synthetic sample source. */
#include "ul_engine.h"
static int fail(struct mt8167_ul_engine *e){e->fault=1;return -1;}
static int rd(struct mt8167_ul_engine *e,uint32_t r,uint32_t *v)
{return e->io.read(e->io.context,r,v);}
static int wr(struct mt8167_ul_engine *e,uint32_t r,uint32_t v)
{return e->io.write(e->io.context,r,v);}
static int bits(struct mt8167_ul_engine *e,uint32_t r,uint32_t mask,uint32_t value)
{uint32_t v;if(rd(e,r,&v)||wr(e,r,(v&~mask)|(value&mask))||rd(e,r,&v)||(v&mask)!=(value&mask))return fail(e);return 0;}
int mt8167_ul_prepare(struct mt8167_ul_engine *e,uint64_t base,uint32_t bytes,uint32_t period)
{
 uint32_t v;
 if(!e||!e->io.read||!e->io.write||e->prepared||e->fault||!base||(base&4095)||
    bytes<8192||bytes>1048576||(bytes&4095)||base>0x100000000ULL-bytes||
    !period||(period&3)||period>bytes/2||bytes%period||period/4>0x3ffff)return -1;
 /* This candidate is exclusive, not a guessed full-duplex clock broker.
  * Check every memif and IRQ1/2 before altering shared route/format bits. */
 if(rd(e,0x10,&v)||(v&0xfe)||rd(e,0x3a0,&v)||(v&0x4003))return -1;
 e->base=(uint32_t)base;e->bytes=bytes;e->period_frames=period/4;
 /* Sticky exposure precedes publishing even a disabled DMA address. */
 e->exposed=1;
 if(wr(e,0x80,e->base)||wr(e,0x88,e->base+bytes-1)||
    rd(e,0x80,&v)||v!=e->base||rd(e,0x88,&v)||v!=e->base+bytes-1||
    bits(e,0x14,(15u<<16)|(1u<<27),10u<<16)||
    bits(e,0x3d8,3u<<22,0)||bits(e,0x6c,3u<<9,0)||
    bits(e,0x2c,9u,9u)||bits(e,0x120,1,0)||
    bits(e,0x114,0x1e0000,0x1e0000)||bits(e,0x13c,0xc00,0xc00)||
    bits(e,0x3b0,0x3ffff,e->period_frames)||bits(e,0x3a0,15u<<8,10u<<8)||
    wr(e,0x3a8,2)||bits(e,0x3b4,2,2))return fail(e);
 e->prepared=1;return 0;
}
int mt8167_ul_start(struct mt8167_ul_engine *e)
{
 uint32_t v;
 if(!e||!e->prepared||e->fault||e->running)return -1;
 if(rd(e,0x10,&v)||(v&0xfe)||rd(e,0x3a0,&v)||(v&0x4003))return fail(e);
 /* Parent has already prepared ADC hardware and codec sample rate. */
 if(bits(e,0x3b4,2,2)||wr(e,0x3a8,2)||bits(e,0x124,1,1)||bits(e,0x114,1,1)||bits(e,0x10,9,9)||bits(e,0x3a0,2,2))return fail(e);
 e->running=1;return 0;
}
int mt8167_ul_stop(struct mt8167_ul_engine *e)
{
 uint32_t v;
 if(!e||!e->prepared||e->fault)return -1;
 if(bits(e,0x10,8,0)||bits(e,0x3a0,2,0)||bits(e,0x3b4,2,0)||
    wr(e,0x3a8,2)||bits(e,0x114,1,0)||rd(e,0x3a4,&v)||(v&2))return fail(e);
 e->running=0;
 /* Do not clear exposure or free mappings: register STOP is not bus drain. */
 return 0;
}
int mt8167_ul_position(struct mt8167_ul_engine *e,uint32_t *offset)
{
 uint32_t v;if(!e||!offset||!e->running||e->fault)return -1;
 if(rd(e,0x8c,&v)||v<e->base||(uint64_t)v>=(uint64_t)e->base+e->bytes||((v-e->base)&3))return fail(e);
 *offset=v-e->base;return 0;
}
int mt8167_ul_interrupt(struct mt8167_ul_engine *e,uint32_t *notify)
{
 uint32_t v;if(!e||!notify||!e->prepared||e->fault)return -1;*notify=0;
 if(rd(e,0x3a4,&v))return fail(e);if(!(v&2))return 0;
 if(wr(e,0x3a8,2))return fail(e);*notify=e->running?1:0;return 1;
}
