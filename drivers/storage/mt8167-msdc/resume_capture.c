/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "resume_capture.h"
#define CFG 0x000u
#define INT 0x00cu
#define INTEN 0x010u
#define FIFO 0x014u
#define RX 0x01cu
#define SDCCFG 0x030u
#define CMD 0x034u
#define ARG 0x038u
#define STS 0x03cu
#define RESP 0x040u
#define BLK 0x050u
#define DMACFG 0x09cu
#define EV 0xed700u
#define CMDEVENTS 0x700u
#define DATAEVENTS 0xd000u
#define FIFO_CLEAR (1u<<31)
struct capture { const struct a04_msdc_io *io; const struct a04_capture_authority *a; uint32_t limit; };
static int allowed(const struct capture *c) { return c->a->permitted(c->a->context,c->a->generation)==1; }
static int expired(const struct capture *c,uint64_t start,uint32_t n)
{ return !(n&63u) && c->a->now(c->a->context)-start>=c->a->ticks; }
static enum a04_capture_result command(struct capture *c,uint32_t word,uint32_t arg,int r1)
{
    const struct a04_msdc_io *io=c->io; uint32_t n,b; uint64_t start;
    start=c->a->now(c->a->context);
    for(n=0;n<c->limit;++n) {
        if(!allowed(c)) return A04_CAPTURE_REVOKED;
        if(!(io->read32(io->context,STS)&1u)) break;
        if(expired(c,start,n)) return A04_CAPTURE_TIMEOUT;
    }
    if(n==c->limit) return A04_CAPTURE_TIMEOUT;
    if(!allowed(c)) return A04_CAPTURE_REVOKED;
    b=io->read32(io->context,INT)&CMDEVENTS;
    if(b) {if(!allowed(c))return A04_CAPTURE_REVOKED;io->write32(io->context,INT,b);}
    if(!allowed(c)) return A04_CAPTURE_REVOKED;
    io->write32(io->context,ARG,arg);
    if(!allowed(c))return A04_CAPTURE_REVOKED;
    io->write32(io->context,CMD,word);
    start=c->a->now(c->a->context);
    for(n=0;n<c->limit;++n) {
        if(!allowed(c)) return A04_CAPTURE_REVOKED;
        b=io->read32(io->context,INT)&CMDEVENTS;
        if(b) {
            if(!allowed(c))return A04_CAPTURE_REVOKED;
            io->write32(io->context,INT,b);
            if((b&0x600u) || !(b&0x100u)) return A04_CAPTURE_COMMAND;
            if(r1) {if(!allowed(c))return A04_CAPTURE_REVOKED;
                if(io->read32(io->context,RESP)&0xffffe0c0u)return A04_CAPTURE_COMMAND;}
            return A04_CAPTURE_OK;
        }
        if(expired(c,start,n)) return A04_CAPTURE_TIMEOUT;
    }
    return A04_CAPTURE_TIMEOUT;
}
static int ready(uint32_t r) { return !(r&0xffffe0c0u) && (r&(1u<<8)) && ((r>>9)&15u)==4; }
static enum a04_capture_result status(struct capture *c,uint32_t *out)
{
    enum a04_capture_result r=command(c,13u|(1u<<7),(uint32_t)c->a->rca<<16,1);
    if(r!=A04_CAPTURE_OK) return r;
    if(!allowed(c)) return A04_CAPTURE_REVOKED;
    *out=c->io->read32(c->io->context,RESP);
    return ready(*out)?A04_CAPTURE_OK:A04_CAPTURE_CARD_STATE;
}
static enum a04_capture_result extcsd(struct capture *c,uint8_t *dst)
{
    const struct a04_msdc_io *io=c->io; uint32_t n,b,count,v; size_t off=0; int done=0;
    uint64_t start; enum a04_capture_result r;
    if(!allowed(c)) return A04_CAPTURE_REVOKED;
    io->write32(io->context,BLK,1);
    if(!allowed(c))return A04_CAPTURE_REVOKED;
    v=io->read32(io->context,FIFO);
    if(!allowed(c))return A04_CAPTURE_REVOKED;
    io->write32(io->context,FIFO,v|FIFO_CLEAR);
    start=c->a->now(c->a->context);
    for(n=0;n<c->limit;++n) {
        if(!allowed(c)) return A04_CAPTURE_REVOKED;
        if(!(io->read32(io->context,FIFO)&FIFO_CLEAR)) break;
        if(expired(c,start,n)) return A04_CAPTURE_TIMEOUT;
    }
    if(n==c->limit) return A04_CAPTURE_TIMEOUT;
    if(!allowed(c))return A04_CAPTURE_REVOKED;
    b=io->read32(io->context,INT)&DATAEVENTS;
    if(b){if(!allowed(c))return A04_CAPTURE_REVOKED;io->write32(io->context,INT,b);}
    r=command(c,8u|(1u<<7)|(1u<<11)|(512u<<16),0,1);
    if(r!=A04_CAPTURE_OK) return r;
    start=c->a->now(c->a->context);
    for(n=0;n<c->limit;++n) {
        if(!allowed(c)) return A04_CAPTURE_REVOKED;
        b=io->read32(io->context,INT)&DATAEVENTS;
        if(b){if(!allowed(c))return A04_CAPTURE_REVOKED;io->write32(io->context,INT,b);}
        if(b&0xc000u) return A04_CAPTURE_DATA;
        if(b&0x1000u) done=1;
        if(!allowed(c))return A04_CAPTURE_REVOKED;
        count=io->read32(io->context,FIFO)&255u;
        while(count && off<512) {
            if(!allowed(c)) return A04_CAPTURE_REVOKED;
            if(count>=4 && 512-off>=4) {
                v=io->read32(io->context,RX); dst[off++]=(uint8_t)v;dst[off++]=(uint8_t)(v>>8);
                dst[off++]=(uint8_t)(v>>16);dst[off++]=(uint8_t)(v>>24);count-=4;
            } else { dst[off++]=io->read8(io->context,RX); --count; }
        }
        if(done && off==512) return A04_CAPTURE_OK;
        if(expired(c,start,n)) return A04_CAPTURE_TIMEOUT;
    }
    return A04_CAPTURE_TIMEOUT;
}
enum a04_capture_result a04_resume_capture(const struct a04_msdc_io *io,
    const struct a04_capture_authority *a,uint32_t limit,uint8_t ext[512],
    struct a04_resume_observation *out)
{
    struct capture c={io,a,limit}; struct a04_resume_observation tmp={0};
    enum a04_capture_result r; uint32_t cfg,host,word,status0; unsigned i,j; uint8_t card,any=0,all=0xffu;
    if(!io||!io->read32||!io->read8||!io->write32||!a||!a->permitted||!a->now||
       !a->ticks||!a->generation||!a->rca||!limit||!ext||!out) return A04_CAPTURE_INVALID;
    if (((uintptr_t)ext <= (uintptr_t)out && (uintptr_t)out-(uintptr_t)ext<512u) ||
        ((uintptr_t)out < (uintptr_t)ext && (uintptr_t)ext-(uintptr_t)out<sizeof(*out)))
        return A04_CAPTURE_INVALID;
    if(!allowed(&c)) return A04_CAPTURE_REVOKED;
    cfg=io->read32(io->context,CFG);
    if(!allowed(&c))return A04_CAPTURE_REVOKED;
    host=(io->read32(io->context,SDCCFG)>>16)&3u;
    if((cfg&0x89u)!=0x89u || host==3)return A04_CAPTURE_UNSAFE;
    if(!allowed(&c))return A04_CAPTURE_REVOKED;
    if(io->read32(io->context,DMACFG)&1u)return A04_CAPTURE_UNSAFE;
    if(!allowed(&c))return A04_CAPTURE_REVOKED;
    if(io->read32(io->context,INTEN)&EV)return A04_CAPTURE_UNSAFE;
    r=status(&c,&status0); if(r!=A04_CAPTURE_OK) return r;
    r=command(&c,10u|(2u<<7),(uint32_t)a->rca<<16,0); if(r!=A04_CAPTURE_OK) return r;
    for(i=0;i<4;++i) {
        if(!allowed(&c)) return A04_CAPTURE_REVOKED;
        word=io->read32(io->context,RESP+4u*(3u-i));
        for(j=0;j<4;++j) {
            tmp.cid[i*4+j]=(uint8_t)(word>>(24u-j*8u));
            any|=tmp.cid[i*4+j]; all&=tmp.cid[i*4+j];
        }
    }
    if(!any || all==0xffu) return A04_CAPTURE_IDENTITY;
    r=extcsd(&c,ext); if(r!=A04_CAPTURE_OK) return r;
    r=status(&c,&tmp.card_status); if(r!=A04_CAPTURE_OK) return r;
    if(!allowed(&c)) return A04_CAPTURE_REVOKED;
    card=ext[183]; if(card>2) return A04_CAPTURE_MODE;
    tmp.host_width=host==0?1u:host==1?4u:8u;
    tmp.card_width=card==0?1u:card==1?4u:8u;
    if(tmp.host_width!=tmp.card_width || (ext[179]&7u) || ext[33]) return A04_CAPTURE_MODE;
    tmp.sectors=(uint32_t)ext[212]|((uint32_t)ext[213]<<8)|((uint32_t)ext[214]<<16)|((uint32_t)ext[215]<<24);
    if(!tmp.sectors) return A04_CAPTURE_IDENTITY;
    /* The admission callback is a software lease, not evidence of powered
     * rails. Recheck the controller at the end, but leave power_owned false
     * until a real power owner supplies independent, generation-bound proof. */
    if(!allowed(&c)) return A04_CAPTURE_REVOKED;
    cfg=io->read32(io->context,CFG);
    if(!allowed(&c)) return A04_CAPTURE_REVOKED;
    if((cfg&0x89u)!=0x89u)return A04_CAPTURE_UNSAFE;
    if(!allowed(&c))return A04_CAPTURE_REVOKED;
    if(((io->read32(io->context,SDCCFG)>>16)&3u)!=host)return A04_CAPTURE_UNSAFE;
    if(!allowed(&c))return A04_CAPTURE_REVOKED;
    if(io->read32(io->context,DMACFG)&1u)return A04_CAPTURE_UNSAFE;
    if(!allowed(&c))return A04_CAPTURE_REVOKED;
    if(io->read32(io->context,INTEN)&EV)return A04_CAPTURE_UNSAFE;
    if(!allowed(&c)) return A04_CAPTURE_REVOKED;
    tmp.generation=a->generation;tmp.rca=a->rca;tmp.partition=ext[179]&7u;tmp.cache_ctrl=ext[33];
    /* CKSTB observes the controller only; the shared source clock and rail
     * are not owned or verified by this capture. */
    tmp.pio_mode=tmp.dma_idle=tmp.events_masked=1;
    *out=tmp;return A04_CAPTURE_OK;
}
