/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "host_prepare.h"
#define CFG 0x000u
#define INT 0x00cu
#define INTEN 0x010u
#define FIFO 0x014u
#define SDC_CFG 0x030u
#define DMA_CFG 0x09cu
#define MODE (1u<<0)
#define RESET (1u<<2)
#define PIO (1u<<3)
#define STABLE (1u<<7)
#define DIV_MASK (0xfffu<<8)
#define CLK_MASK ((3u<<20)|(1u<<22)|DIV_MASK)
#define FIFO_CLEAR (1u<<31)
#define WIDTH_MASK (3u<<16)
#define DMA_ACTIVE 1u
#define TARGET_HZ 260000u
static enum a04_host_prepare_result fail(struct a04_host_prepare_state *s,enum a04_host_prepare_result r)
{ s->poisoned=1;return r; }
static int owner(const struct a04_host_prepare_ops *o,uint32_t g)
{ return o->owned_and_quiescent(o->context,g)==1; }
static enum a04_host_prepare_result wait_bits(const struct a04_host_prepare_ops *o,uint32_t g,
    uint32_t reg,uint32_t mask,int set,uint32_t limit)
{
    uint64_t start=o->now(o->context);uint32_t v;
    while(limit--) {
        if(!owner(o,g))return A04_HOST_UNOWNED;
        if(o->now(o->context)-start>=o->timeout_ticks)return A04_HOST_TIMEOUT;
        v=o->io->read32(o->io->context,reg);
        if(v==UINT32_MAX)return A04_HOST_VERIFY;
        if(set ? (v&mask)==mask : !(v&mask))return A04_HOST_OK;
    }
    return A04_HOST_TIMEOUT;
}
enum a04_host_prepare_result a04_host_prepare(struct a04_host_prepare_state *s,
    const struct a04_host_prepare_ops *o,uint32_t g,uint32_t limit)
{
    const struct a04_msdc_io *io;uint32_t div,v;enum a04_host_prepare_result r;
    if(!s||!o||!o->io||!o->io->read32||!o->io->write32||!o->owned_and_quiescent||
       !o->gate_clock||!o->verify_source||!o->now||!o->timeout_ticks||!o->source_hz_max||!g||!limit)
        return A04_HOST_INVALID;
    if(s->poisoned)return A04_HOST_POISONED;
    /* ceil(source/(4*260k)); div=0 has different hardware semantics. */
    div=o->source_hz_max/(4u*TARGET_HZ);
    if(o->source_hz_max%(4u*TARGET_HZ))++div;
    if(!div||div>0xfffu)return A04_HOST_INVALID;
    io=o->io;
    if(!owner(o,g))return A04_HOST_UNOWNED;
    if(o->verify_source(o->context,g,o->source_hz_max)!=1)return A04_HOST_CLOCK;
    if(!owner(o,g))return A04_HOST_UNOWNED;
    if(io->read32(io->context,DMA_CFG)&DMA_ACTIVE)return A04_HOST_DMA_ACTIVE;
    if(!owner(o,g))return A04_HOST_UNOWNED;
    io->write32(io->context,INTEN,0);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,CFG);
    if(v==UINT32_MAX)return fail(s,A04_HOST_VERIFY);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    io->write32(io->context,CFG,v|RESET);
    r=wait_bits(o,g,CFG,RESET,0,limit);if(r!=A04_HOST_OK)return fail(s,r);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,FIFO);
    if(v==UINT32_MAX)return fail(s,A04_HOST_VERIFY);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    io->write32(io->context,FIFO,v|FIFO_CLEAR);
    r=wait_bits(o,g,FIFO,FIFO_CLEAR,0,limit);if(r!=A04_HOST_OK)return fail(s,r);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,INT);
    if(v==UINT32_MAX)return fail(s,A04_HOST_VERIFY);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    io->write32(io->context,INT,v);
    if(!owner(o,g)||o->gate_clock(o->context,g,1)!=1)return fail(s,A04_HOST_CLOCK);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,CFG);
    if(v==UINT32_MAX)return fail(s,A04_HOST_VERIFY);
    v=(v&~CLK_MASK)|(div<<8)|MODE|PIO;
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    io->write32(io->context,CFG,v);
    if(!owner(o,g)||o->gate_clock(o->context,g,0)!=1)return fail(s,A04_HOST_CLOCK);
    r=wait_bits(o,g,CFG,STABLE,1,limit);if(r!=A04_HOST_OK)return fail(s,r);
    if(!owner(o,g)||o->verify_source(o->context,g,o->source_hz_max)!=1)return fail(s,A04_HOST_CLOCK);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,SDC_CFG);
    if(v==UINT32_MAX)return fail(s,A04_HOST_VERIFY);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    io->write32(io->context,SDC_CFG,v&~WIDTH_MASK);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,CFG);
    if(v==UINT32_MAX || (v&(CLK_MASK|MODE|PIO|RESET|STABLE))!=((div<<8)|MODE|PIO|STABLE))
        return fail(s,A04_HOST_VERIFY);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,SDC_CFG);
    if(v==UINT32_MAX || (v&WIDTH_MASK))return fail(s,A04_HOST_VERIFY);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,DMA_CFG);
    if(v==UINT32_MAX || (v&DMA_ACTIVE))return fail(s,A04_HOST_VERIFY);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    v=io->read32(io->context,INTEN);
    if(v!=0)return fail(s,A04_HOST_VERIFY);
    if(!owner(o,g))return fail(s,A04_HOST_UNOWNED);
    return A04_HOST_OK;
}
