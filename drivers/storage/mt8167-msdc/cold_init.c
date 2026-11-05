/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "cold_init.h"
#define CFG 0x000u
#define INT 0x00cu
#define INTEN 0x010u
#define SDCCFG 0x030u
#define CMD 0x034u
#define ARG 0x038u
#define STS 0x03cu
#define RESP 0x040u
#define DMACFG 0x09cu
#define EVENTS 0xed700u
#define COMMAND_EVENTS 0x700u
#define OCR_ARG 0x40ff8000u
#define OCR_READY (1u<<31)
#define OCR_SECTOR (1u<<30)
struct init { const struct a04_msdc_io *io;const struct a04_cold_authority *a;uint32_t limit;uint64_t start; };
static int allowed(struct init *s) { return s->a->prepare_valid(s->a->context,s->a->generation)==1; }
static int expired(struct init *s) { return s->a->now(s->a->context)-s->start>=s->a->ticks; }
static enum a04_cold_result send(struct init *s,uint32_t word,uint32_t arg,int r1)
{
    const struct a04_msdc_io *io=s->io;uint32_t n,b;
    if(expired(s)) return A04_COLD_TIMEOUT;
    for(n=0;n<s->limit;++n) {
        if(!allowed(s)) return A04_COLD_REVOKED;
        if(!(io->read32(io->context,STS)&1u)) break;
        if(!(n&63u) && expired(s)) return A04_COLD_TIMEOUT;
    }
    if(n==s->limit) return A04_COLD_TIMEOUT;
    if(!allowed(s)) return A04_COLD_REVOKED;
    b=io->read32(io->context,INT)&COMMAND_EVENTS;
    if(b){if(!allowed(s))return A04_COLD_REVOKED;io->write32(io->context,INT,b);}
    if(!allowed(s)) return A04_COLD_REVOKED;
    io->write32(io->context,ARG,arg);
    if(!allowed(s))return A04_COLD_REVOKED;
    io->write32(io->context,CMD,word);
    for(n=0;n<s->limit;++n) {
        if(!allowed(s)) return A04_COLD_REVOKED;
        b=io->read32(io->context,INT)&COMMAND_EVENTS;
        if(b) {
            if(!allowed(s))return A04_COLD_REVOKED;
            io->write32(io->context,INT,b);
            if((b&0x600u) || !(b&0x100u))return A04_COLD_COMMAND;
            if(r1) {if(!allowed(s))return A04_COLD_REVOKED;
                if(io->read32(io->context,RESP)&0xffffe0c0u)return A04_COLD_COMMAND;}
            if(expired(s))return A04_COLD_TIMEOUT;
            return A04_COLD_OK;
        }
        if(!(n&63u) && expired(s)) return A04_COLD_TIMEOUT;
    }
    return A04_COLD_TIMEOUT;
}
static int cap_permitted(void *p,uint32_t generation)
{ struct init *s=p; return generation==s->a->generation && allowed(s) && !expired(s); }
static uint64_t cap_now(void *p)
{ struct init *s=p;return s->a->now(s->a->context); }
enum a04_cold_result a04_emmc_cold_init(const struct a04_msdc_io *io,
    const struct a04_cold_authority *a,uint32_t limit,uint8_t scratch[512],
    struct a04_resume_observation *out)
{
    struct init s;struct a04_resume_observation tmp;struct a04_capture_authority cap;
    enum a04_cold_result r;uint32_t n,b,word;uint8_t cid[16];unsigned i,j;
    if(!io||!io->read32||!io->read8||!io->write32||!a||!a->prepare_valid||!a->now||
       !a->delay_us||!a->ticks||!a->generation||!a->rca||!limit||!scratch||!out) return A04_COLD_INVALID;
    if (((uintptr_t)scratch <= (uintptr_t)out && (uintptr_t)out-(uintptr_t)scratch<512u) ||
        ((uintptr_t)out < (uintptr_t)scratch && (uintptr_t)scratch-(uintptr_t)out<sizeof(*out))) return A04_COLD_INVALID;
    s.io=io;s.a=a;s.limit=limit;s.start=a->now(a->context);
    if(!allowed(&s)) return A04_COLD_REVOKED;
    if((io->read32(io->context,CFG)&0x89u)!=0x89u)return A04_COLD_UNSAFE;
    if(!allowed(&s))return A04_COLD_REVOKED;
    if(io->read32(io->context,SDCCFG)&0x30000u)return A04_COLD_UNSAFE;
    if(!allowed(&s))return A04_COLD_REVOKED;
    if(io->read32(io->context,DMACFG)&1u)return A04_COLD_UNSAFE;
    if(!allowed(&s))return A04_COLD_REVOKED;
    if(io->read32(io->context,INTEN)&EVENTS)return A04_COLD_UNSAFE;
    r=send(&s,0,0,0);if(r!=A04_COLD_OK)return r;
    if(!allowed(&s))return A04_COLD_REVOKED;
    a->delay_us(a->context,1000);
    for(n=0;n<1000u;++n) {
        if(expired(&s))return A04_COLD_TIMEOUT;
        r=send(&s,1u|(3u<<7),OCR_ARG,0);if(r!=A04_COLD_OK)return r;
        if(!allowed(&s))return A04_COLD_REVOKED;
        b=io->read32(io->context,RESP);
        if(b&OCR_READY)break;
        a->delay_us(a->context,10000);
    }
    if(n==1000u)return A04_COLD_TIMEOUT;
    if(!(b&OCR_SECTOR))return A04_COLD_OCR;
    r=send(&s,2u|(2u<<7),0,0);if(r!=A04_COLD_OK)return r;
    for(i=0;i<4;++i) {
        if(!allowed(&s))return A04_COLD_REVOKED;
        word=io->read32(io->context,RESP+4u*(3u-i));
        for(j=0;j<4;++j)cid[i*4+j]=(uint8_t)(word>>(24u-j*8u));
    }
    r=send(&s,3u|(1u<<7),(uint32_t)a->rca<<16,1);if(r!=A04_COLD_OK)return r;
    r=send(&s,9u|(2u<<7),(uint32_t)a->rca<<16,0);if(r!=A04_COLD_OK)return r;
    r=send(&s,7u|(1u<<7),(uint32_t)a->rca<<16,1);if(r!=A04_COLD_OK)return r;
    r=send(&s,16u|(1u<<7),512,1);if(r!=A04_COLD_OK)return r;
    if(!allowed(&s))return A04_COLD_REVOKED;
    if(expired(&s))return A04_COLD_TIMEOUT;
    cap.context=&s;cap.permitted=cap_permitted;cap.now=cap_now;
    cap.ticks=a->ticks;cap.generation=a->generation;cap.rca=a->rca;
    if(a04_resume_capture(io,&cap,limit,scratch,&tmp)!=A04_CAPTURE_OK) {
        if(!allowed(&s))return A04_COLD_REVOKED;
        if(expired(&s))return A04_COLD_TIMEOUT;
        return A04_COLD_CAPTURE;
    }
    for(i=0;i<16;++i)if(cid[i]!=tmp.cid[i])return A04_COLD_CID_MISMATCH;
    if(!allowed(&s))return A04_COLD_REVOKED;
    if(expired(&s))return A04_COLD_TIMEOUT;
    *out=tmp;return A04_COLD_OK;
}
