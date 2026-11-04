/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "sync_pio.h"
#define CFG 0x000u
#define PIN 0x008u
#define INT 0x00cu
#define INTEN 0x010u
#define FIFO 0x014u
#define RX 0x01cu
#define CMD 0x034u
#define ARG 0x038u
#define STS 0x03cu
#define RESP 0x040u
#define BLK 0x050u
#define DMACFG 0x09cu
#define CMD_BITS 0x700u
#define DATA_BITS 0xd000u
#define EVENTS (CMD_BITS | DATA_BITS | 0xe0000u)
#define READY 0x100u
#define DONE 0x1000u
#define DATA_ERR 0xc000u
#define CLEAR (1u<<31)
#define DAT0 (1u<<16)
struct timer { const struct a04_sync_clock *c; uint64_t start; };
static int expired(const struct timer *t,uint32_t n)
{ return t->c && !(n & 63u) && t->c->now(t->c->context)-t->start>=t->c->ticks; }
static void reset_timer(struct timer *t)
{ if(t->c) t->start=t->c->now(t->c->context); }
static enum a04_sync_result command(const struct a04_msdc_io *io,
    uint32_t word,uint32_t arg,uint32_t limit,struct timer *t)
{
    uint32_t n,b;
    reset_timer(t);
    for(n=0;n<limit;++n) {
        if(!(io->read32(io->context,STS)&1u)) break;
        if(expired(t,n)) return A04_SYNC_TIMEOUT;
    }
    if(n==limit) return A04_SYNC_TIMEOUT;
    b=io->read32(io->context,INT)&CMD_BITS;
    if(b) io->write32(io->context,INT,b);
    io->write32(io->context,ARG,arg); io->write32(io->context,CMD,word);
    reset_timer(t);
    for(n=0;n<limit;++n) {
        b=io->read32(io->context,INT)&CMD_BITS;
        if(b) {
            io->write32(io->context,INT,b);
            if((b&0x600u) || !(b&READY) || (io->read32(io->context,RESP)&0xffff0000u))
                return A04_SYNC_COMMAND_ERROR;
            return A04_SYNC_OK;
        }
        if(expired(t,n)) return A04_SYNC_TIMEOUT;
    }
    return A04_SYNC_TIMEOUT;
}
static int stop(const struct a04_msdc_io *io,uint32_t limit,struct timer *t)
{
    uint32_t n;
    if(command(io,12u|(7u<<7)|(1u<<14),0,limit,t)!=A04_SYNC_OK) return 0;
    reset_timer(t);
    for(n=0;n<limit;++n) {
        if(io->read32(io->context,PIN)&DAT0) return 1;
        if(expired(t,n)) return 0;
    }
    return 0;
}
enum a04_sync_result a04_sync_pio_read(const struct a04_msdc_io *io,
    uint32_t lba,uint32_t blocks,uint8_t *staging,size_t staging_len,
    uint8_t *output,size_t output_len,uint32_t limit,const struct a04_sync_clock *clock)
{
    uint32_t n,b,count,val; size_t offset=0,total; int done=0,started=0;
    enum a04_sync_result r; struct timer t={clock,0};
    if(!io || !io->read32 || !io->read8 || !io->write32 || !staging || !output ||
       !blocks || blocks>8 || lba>UINT32_MAX-(blocks-1) || !limit ||
       (clock && (!clock->now || !clock->ticks))) return A04_SYNC_INVALID;
    total=(size_t)blocks*512u;
    if(staging_len<total || output_len<total || staging==output) return A04_SYNC_INVALID;
    /* Pointer addition can overflow; use differences to reject all overlap. */
    if (((uintptr_t)staging <= (uintptr_t)output && (uintptr_t)output-(uintptr_t)staging<total) ||
        ((uintptr_t)output < (uintptr_t)staging && (uintptr_t)staging-(uintptr_t)output<total)) return A04_SYNC_INVALID;
    if(!(io->read32(io->context,CFG)&8u) || (io->read32(io->context,DMACFG)&1u) ||
       (io->read32(io->context,INTEN)&EVENTS)) return A04_SYNC_BUSY;
    if(blocks>1) { r=command(io,23u|(1u<<7),blocks,limit,&t); if(r!=A04_SYNC_OK) return r; }
    io->write32(io->context,BLK,blocks);
    io->write32(io->context,FIFO,io->read32(io->context,FIFO)|CLEAR);
    reset_timer(&t);
    for(n=0;n<limit;++n) { if(!(io->read32(io->context,FIFO)&CLEAR)) break; if(expired(&t,n)) return A04_SYNC_TIMEOUT; }
    if(n==limit) return A04_SYNC_TIMEOUT;
    b=io->read32(io->context,INT)&DATA_BITS; if(b) io->write32(io->context,INT,b);
    started=1;
    r=command(io,(blocks>1?18u:17u)|(1u<<7)|((blocks>1?2u:1u)<<11)|(512u<<16),lba,limit,&t);
    if(r!=A04_SYNC_OK) goto failed;
    reset_timer(&t);
    for(n=0;n<limit;++n) {
        b=io->read32(io->context,INT)&DATA_BITS; if(b) io->write32(io->context,INT,b);
        if(b&DATA_ERR) { r=A04_SYNC_DATA_ERROR; goto failed; }
        if(b&DONE) done=1;
        count=io->read32(io->context,FIFO)&0xffu;
        while(count && offset<total) {
            if(count>=4 && total-offset>=4) {
                val=io->read32(io->context,RX);
                staging[offset++]=(uint8_t)val; staging[offset++]=(uint8_t)(val>>8);
                staging[offset++]=(uint8_t)(val>>16); staging[offset++]=(uint8_t)(val>>24); count-=4;
            } else { staging[offset++]=io->read8(io->context,RX); --count; }
        }
        if(done && offset==total) { for(offset=0;offset<total;++offset) output[offset]=staging[offset]; return A04_SYNC_OK; }
        if(expired(&t,n)) break;
    }
    r=A04_SYNC_TIMEOUT;
failed:
    if(started && blocks>1 && !stop(io,limit,&t)) return A04_SYNC_STOP_ERROR;
    return r;
}
