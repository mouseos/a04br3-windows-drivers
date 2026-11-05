/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_CODEC_DOMAIN_H
#define A04_MT8167_CODEC_DOMAIN_H
#include <stdint.h>
/* Linux MT8167 CODEC_CLK, DL_CLK, Vcm14 and DAC/voice DAPM supplies. This is only the common
 * clock/reference domain; it does not enable the DAC or prove the output. */
struct a04_codec_domain_io {
 void *context;int (*read)(void *,unsigned,uint32_t *);int (*write)(void *,unsigned,uint32_t);
};
struct a04_codec_domain { uint32_t saved[8];int held,poisoned; };
static int a04_codec_domain_update(const struct a04_codec_domain_io *io,
                                  unsigned offset,uint32_t mask,uint32_t expected,uint32_t target)
{
 uint32_t value,after;
 if(io->read(io->context,offset,&value) || (value&mask)!=expected)return -1;
 if(expected!=target && (io->write(io->context,offset,(value&~mask)|target) ||
    io->read(io->context,offset,&after) || (after&mask)!=target))return -1;
 return 0;
}
static int a04_codec_domain_start(struct a04_codec_domain *s,const struct a04_codec_domain_io *io)
{
 static const unsigned off[8]={0x704u,0x70cu,0x710u,0x700u,0x708u,0x708u,0x704u,0x708u};
 static const uint32_t mask[8]={1u<<20,1u<<30,1u<<15,(1u<<19)|(1u<<1),1u<<16,1u<<27,1u<<13,1u<<17};
 unsigned i;uint32_t value;
 if(!s || !io || !io->read || !io->write || s->held || s->poisoned)return -1;
 for(i=0;i<8;i++) {if(io->read(io->context,off[i],&value))return -1;s->saved[i]=value&mask[i];}
 s->held=1;
 for(i=0;i<8;i++)if(a04_codec_domain_update(io,off[i],mask[i],s->saved[i],mask[i])){s->poisoned=1;return -1;}
 return 0;
}
static int a04_codec_domain_stop(struct a04_codec_domain *s,const struct a04_codec_domain_io *io)
{
 static const unsigned off[8]={0x704u,0x70cu,0x710u,0x700u,0x708u,0x708u,0x704u,0x708u};
 static const uint32_t mask[8]={1u<<20,1u<<30,1u<<15,(1u<<19)|(1u<<1),1u<<16,1u<<27,1u<<13,1u<<17};
 unsigned i;
 if(!s || !io || !io->read || !io->write || !s->held || s->poisoned)return -1;
 for(i=8;i>0;i--)if(a04_codec_domain_update(io,off[i-1],mask[i-1],mask[i-1],s->saved[i-1])){s->poisoned=1;return -1;}
 s->held=0;return 0;
}
#endif
