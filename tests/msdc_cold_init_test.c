/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "cold_init.h"
#include <assert.h>
#include <string.h>
struct fake { uint32_t cfg,sdccfg,inten,dma,ints,cmd,arg,commands[32],args[32],ncmd,pos,ticks,checks,deny_at,ocr_count,fail_cmd,cid_mismatch,delay_us,bad_ocr;uint8_t ext[512]; };
static uint32_t rd32(void *p,uint32_t off)
{
    struct fake *f=p;
    if(off==0)return f->cfg;
    if(off==0x30)return f->sdccfg;
    if(off==0x10)return f->inten;
    if(off==0x9c)return f->dma;
    if(off==0x3c)return 0;
    if(off==0xc)return f->ints | (f->cmd==8 && f->pos==512 ? 0x1000u:0);
    if(off==0x14)return f->cmd==8 && f->pos<512?64:0;
    if(off==0x1c) { uint32_t v=0;unsigned j;for(j=0;j<4;++j)v|=(uint32_t)f->ext[f->pos++]<<(8*j);return v; }
    if(off>=0x40 && off<=0x4c) {
        if(f->cmd==1) return f->ocr_count<2 ? 0 : (f->bad_ocr ? 0x80ff8000u : 0xc0ff8000u);
        if(f->cmd==2 || f->cmd==10) return 0x10203040u+(off-0x40)+(f->cmd==10 && f->cid_mismatch ? 1u:0u);
        return (4u<<9)|(1u<<8);
    }
    return 0;
}
static uint8_t rd8(void *p,uint32_t off) {struct fake *f=p;assert(off==0x1c);return f->ext[f->pos++];}
static void wr32(void *p,uint32_t off,uint32_t v)
{
    struct fake *f=p;assert(off==0xc||off==0x14||off==0x34||off==0x38||off==0x50);
    if(off==0xc)f->ints&=~v;
    if(off==0x38)f->arg=v;
    if(off==0x34) {assert(f->ncmd<32);f->cmd=v&63u;f->commands[f->ncmd]=v;f->args[f->ncmd++]=f->arg;if(f->cmd==1)f->ocr_count++;f->ints|=(f->fail_cmd && f->cmd==f->fail_cmd)?0x400u:0x100u;}
}
static int allowed(void *p,uint32_t generation) {struct fake *f=p;assert(generation==2);return !(f->deny_at && ++f->checks>=f->deny_at);}
static uint64_t now(void *p) {return ((struct fake *)p)->ticks++;}
static void delay(void *p,uint32_t us) {struct fake *f=p;assert(us==1000||us==10000);f->delay_us+=us;}
static void init(struct fake *f) {memset(f,0,sizeof(*f));f->cfg=0x89;f->ext[212]=0x39;f->ext[213]=0x30;}
int main(void)
{
    struct fake f;struct a04_msdc_io io={&f,rd32,rd8,wr32};struct a04_cold_authority a={&f,allowed,now,delay,100000,2,1};
    struct a04_resume_observation out,old;uint8_t scratch[512];unsigned i;
    const uint8_t expected[]={0,1,1,2,3,9,7,16,13,10,8,13};
    init(&f);memset(&out,0xa5,sizeof(out));old=out;
    assert(a04_emmc_cold_init(&io,&a,1000,scratch,&out)==A04_COLD_OK);
    assert(f.ncmd==sizeof(expected) && f.delay_us==11000);
    for(i=0;i<sizeof(expected);++i) assert((f.commands[i]&63u)==expected[i]);
    assert(f.args[1]==0x40ff8000u && f.args[4]==65536 && f.args[7]==512);
    assert(out.sectors==12345 && out.host_width==1 && out.card_width==1 && out.generation==2);
    init(&f);f.cfg=0;out=old;assert(a04_emmc_cold_init(&io,&a,1000,scratch,&out)==A04_COLD_UNSAFE && !f.ncmd && !memcmp(&out,&old,sizeof out));
    init(&f);f.deny_at=1;assert(a04_emmc_cold_init(&io,&a,1000,scratch,&out)==A04_COLD_REVOKED && !f.ncmd && !memcmp(&out,&old,sizeof out));
    init(&f);f.bad_ocr=1;assert(a04_emmc_cold_init(&io,&a,1000,scratch,&out)==A04_COLD_OCR && f.ncmd==3 && !memcmp(&out,&old,sizeof out));
    init(&f);f.fail_cmd=3;assert(a04_emmc_cold_init(&io,&a,1000,scratch,&out)==A04_COLD_COMMAND && f.ncmd==5 && !memcmp(&out,&old,sizeof out));
    init(&f);f.cid_mismatch=1;assert(a04_emmc_cold_init(&io,&a,1000,scratch,&out)==A04_COLD_CID_MISMATCH && !memcmp(&out,&old,sizeof out));
    init(&f);a.ticks=1;assert(a04_emmc_cold_init(&io,&a,1000,scratch,&out)==A04_COLD_TIMEOUT && !memcmp(&out,&old,sizeof out));
    return 0;
}
