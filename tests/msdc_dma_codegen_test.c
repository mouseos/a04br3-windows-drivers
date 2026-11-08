/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Two separately compiled, unchanged DMA cores: -O0 and -O2. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dma.h"

enum a04_dma_result dma_o0(const struct a04_msdc_io *, uint32_t, uint64_t,
        uint32_t, uint32_t, int, uint16_t, const struct a04_dma_clock *);
enum a04_dma_result dma_o2(const struct a04_msdc_io *, uint32_t, uint64_t,
        uint32_t, uint32_t, int, uint16_t, const struct a04_dma_clock *);
enum { NORMAL, FIFO_STUCK, CMD_MISSING, CMD_TIMEOUT, CMD_CRC, CARD_ERROR,
       DATA_MISSING, DATA_TIMEOUT, DATA_CRC, STOP_STUCK, DAT0_STUCK,
       STATUS_STUCK, CMD13_MISSING, CMD13_TIMEOUT, CMD13_CRC, CMD13_BAD,
       INITIAL_IRQ, INITIAL_BUSY, INITIAL_DMA, SCENARIOS };
struct event { uint64_t value; uint32_t kind, offset; };
struct fake {
    uint32_t regs[64], scenario, delay, command, reads[7], active, stopped;
    uint64_t time;
    unsigned count;
    struct event trace[60000];
};
static void log_event(struct fake *f, uint32_t kind, uint32_t off, uint64_t value)
{
    assert(f->count < sizeof(f->trace)/sizeof(f->trace[0]));
    f->trace[f->count++] = (struct event){value,kind,off};
}
static uint32_t rd(void *p, uint32_t off)
{
    struct fake *f=p; uint32_t v=f->regs[off/4], s=f->scenario;
    if(off==0x014 && s!=FIFO_STUCK && ++f->reads[0]>=f->delay) v&=~(1u<<31);
    if(off==0x00c && f->command && !f->active) {
        unsigned c=f->command==13, phase=c?2:1;
        if(++f->reads[phase]>=f->delay && s!=(c?CMD13_MISSING:CMD_MISSING))
            v|=s==(c?CMD13_TIMEOUT:CMD_TIMEOUT)?1u<<9:s==(c?CMD13_CRC:CMD_CRC)?1u<<10:1u<<8;
    }
    if(off==0x00c && f->active && !f->stopped && ++f->reads[3]>=f->delay && s!=DATA_MISSING)
        v|=s==DATA_TIMEOUT?1u<<14:s==DATA_CRC?1u<<15:1u<<12;
    if(off==0x09c && f->stopped && s!=STOP_STUCK && ++f->reads[4]>=f->delay) v&=~1u;
    if(off==0x008 && f->stopped) v=s!=DAT0_STUCK && ++f->reads[5]>=f->delay?1u<<16:0;
    if(off==0x03c && f->stopped) v=s==STATUS_STUCK || ++f->reads[6]<f->delay?1u:0;
    if(off==0x040) v=f->command==13?(s==CMD13_BAD?0:(1u<<8)|(4u<<9)):(s==CARD_ERROR?1u<<16:0);
    f->regs[off/4]=v;log_event(f,0,off,v);return v;
}
static void wr(void *p,uint32_t off,uint32_t v)
{
    struct fake *f=p;log_event(f,1,off,v);
    if(off==0x00c) f->regs[off/4]&=~v;
    else f->regs[off/4]=v;
    if(off==0x034) {f->command=v&63u;if(f->command==13) f->active=0;}
    if(off==0x098 && (v&1)) {f->active=1;f->regs[0x09c/4]=1;}
    if(off==0x098 && (v&2)) f->stopped=1;
}
static uint64_t now(void *p)
{struct fake *f=p;uint64_t v=f->time++;log_event(f,2,0,v);return v;}
static void observe(void *p,uint32_t polls)
{log_event(p,3,0,polls);}
static void init(struct fake *f,unsigned scenario,unsigned delay,uint64_t time)
{
    memset(f,0,sizeof(*f));f->scenario=scenario;f->delay=delay;f->time=time;
    f->regs[0]=9;f->regs[0x008/4]=1u<<16;
    if(scenario==INITIAL_IRQ)f->regs[0x010/4]=1u<<12;
    if(scenario==INITIAL_BUSY)f->regs[0x03c/4]=1;
    if(scenario==INITIAL_DMA)f->regs[0x09c/4]=1;
}
int main(void)
{
    static struct fake a,b;unsigned s,d,w,c,t,tests=0,stops=0;
    const unsigned delays[]={1,2,1025,4097};
    const uint64_t times[]={0,UINT64_MAX-2};
    for(s=0;s<SCENARIOS;s++)for(d=0;d<4;d++)for(w=0;w<2;w++)for(c=0;c<3;c++)for(t=0;t<2;t++) {
        struct a04_msdc_io x={&a,rd,0,wr}, y={&b,rd,0,wr};
        struct a04_dma_clock cx={&a,now,c==1?3:100,observe},cy={&b,now,c==1?3:100,observe};
        enum a04_dma_result r0,r2;
        init(&a,s,delays[d],times[t]);init(&b,s,delays[d],times[t]);
        r0=dma_o0(&x,42,0x100040,64,4500,w,1,c?&cx:0);
        r2=dma_o2(&y,42,0x100040,64,4500,w,1,c?&cy:0);
        assert(r0==r2 && a.count==b.count);
        assert(!memcmp(&a,&b,sizeof(a)));
        if(s==NORMAL && (d<2 || c!=1))assert(r0==A04_DMA_OK);
        if(s==STOP_STUCK && d==0){assert(r0==A04_DMA_STOP_TIMEOUT);stops++;}
        if(s==INITIAL_IRQ || s==INITIAL_BUSY || s==INITIAL_DMA)assert(r0==A04_DMA_BUSY);
        tests++;
    }
    /* Invalid addresses/alignment/clock must not start hardware. */
    for(s=0;s<6;s++) {
        struct a04_msdc_io x={&a,rd,0,wr},y={&b,rd,0,wr};
        struct a04_dma_clock cx={&a,now,0,0},cy={&b,now,0,0};
        uint64_t addr=s==0?0x100000000ULL:s==1?0x100041:0x100040;
        uint32_t alignment=s==2?3:64,limit=s==3?0:10;
        init(&a,NORMAL,1,0);init(&b,NORMAL,1,0);
        assert(dma_o0(&x,0,addr,alignment,limit,s==4,0,s==5?&cx:0)==A04_DMA_INVALID);
        assert(dma_o2(&y,0,addr,alignment,limit,s==4,0,s==5?&cy:0)==A04_DMA_INVALID);
        assert(a.count==0 && b.count==0);tests++;
    }
    assert(stops==12);
    printf("{\"cases\":%u,\"stop_failure_cases_asserted\":%u,\"full_MMIO_clock_observe_trace_equal\":true,\"hardware_test\":false}\n",tests,stops);
    return 0;
}
