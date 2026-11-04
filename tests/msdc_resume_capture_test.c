/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "resume_capture.h"
#include <assert.h>
#include <string.h>
struct fake { uint32_t cfg,sdccfg,inten,dma,ints,cmd,arg,commands[8],args[8],ncmd,pos,ticks,revoked,checks,revoke_at,invalid_cid,late_clock_loss,late_width,late_dma,late_interrupt; uint8_t ext[512]; };
static uint32_t rd32(void *p,uint32_t off)
{
    struct fake *f=p;
    if(off==0) return f->late_clock_loss && f->ncmd>=4 ? f->cfg & ~0x80u : f->cfg;
    if(off==0x30) return f->late_width && f->ncmd>=4 ? 0 : f->sdccfg;
    if(off==0x10) return f->late_interrupt && f->ncmd>=4 ? 0x1000u : f->inten;
    if(off==0x9c) return f->late_dma && f->ncmd>=4 ? 1u : f->dma;
    if(off==0x3c) return 0;
    if(off==0xc) return f->ints | (f->cmd==8 && f->pos==512 ? 0x1000u:0);
    if(off==0x14) return f->cmd==8 && f->pos<512?64:0;
    if(off==0x1c) { uint32_t x=0;unsigned j;for(j=0;j<4;j++)x|=(uint32_t)f->ext[f->pos++]<<(8*j);return x; }
    if(off>=0x40 && off<=0x4c) return f->cmd==10 ? (f->invalid_cid ? 0 : 0x10203040u+(off-0x40)) : (4u<<9)|(1u<<8);
    return 0;
}
static uint8_t rd8(void *p,uint32_t off) { struct fake *f=p;assert(off==0x1c);return f->ext[f->pos++]; }
static void wr32(void *p,uint32_t off,uint32_t v)
{
    struct fake *f=p;
    assert(off==0xc || off==0x14 || off==0x34 || off==0x38 || off==0x50);
    if(off==0xc) f->ints &= ~v;
    if(off==0x38) f->arg=v;
    if(off==0x34) { assert(f->ncmd<8);f->cmd=v&63u;f->commands[f->ncmd]=v;f->args[f->ncmd++]=f->arg;f->ints|=0x100u; }
}
static int permit(void *p,uint32_t g) { struct fake *f=p;assert(g==2);f->checks++;if(f->revoke_at && f->checks>=f->revoke_at)f->revoked=1;return !f->revoked; }
static uint64_t now(void *p) { struct fake *f=p;return f->ticks++; }
static void init(struct fake *f) { memset(f,0,sizeof(*f));f->cfg=0x89;f->sdccfg=2u<<16;f->ext[183]=2;f->ext[212]=0x39;f->ext[213]=0x30; }
int main(void)
{
    struct fake f;struct a04_msdc_io io={&f,rd32,rd8,wr32};struct a04_capture_authority a={&f,permit,now,100,2,1};
    struct a04_resume_observation out,old;uint8_t scratch[512];
    init(&f);memset(&out,0xa5,sizeof(out));
    assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_OK);
    assert(!out.power_owned && out.clock_stable==1 && out.pio_mode==1 && out.dma_idle==1 && out.events_masked==1);
    assert(f.ncmd==4 && (f.commands[0]&63)==13 && (f.commands[1]&63)==10 && (f.commands[2]&63)==8 && (f.commands[3]&63)==13);
    assert(f.args[0]==65536 && f.args[1]==65536 && f.args[2]==0 && f.args[3]==65536);
    assert(out.sectors==12345 && out.generation==2 && out.host_width==8 && out.card_width==8 && out.rca==1 && out.cid[0]==0x10 && out.cid[3]==0x4c);
    init(&f);f.dma=1;memset(&out,0xa5,sizeof(out));old=out;
    assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_UNSAFE && !f.ncmd && !memcmp(&out,&old,sizeof(out)));
    init(&f);f.inten=0x1000;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_UNSAFE && !f.ncmd);
    init(&f);f.late_clock_loss=1;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_UNSAFE && !memcmp(&out,&old,sizeof(out)));
    init(&f);f.late_width=1;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_UNSAFE && !memcmp(&out,&old,sizeof(out)));
    init(&f);f.late_dma=1;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_UNSAFE && !memcmp(&out,&old,sizeof(out)));
    init(&f);f.late_interrupt=1;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_UNSAFE && !memcmp(&out,&old,sizeof(out)));
    init(&f);f.invalid_cid=1;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_IDENTITY && !memcmp(&out,&old,sizeof(out)));
    init(&f);f.ext[183]=1;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_MODE && !memcmp(&out,&old,sizeof(out)));
    init(&f);f.ext[179]=1;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_MODE);
    init(&f);f.revoke_at=10;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_REVOKED && !memcmp(&out,&old,sizeof(out)));
    init(&f);f.revoked=1;assert(a04_resume_capture(&io,&a,1000,scratch,&out)==A04_CAPTURE_REVOKED && !f.ncmd);
    init(&f);assert(a04_resume_capture(&io,&a,1000,scratch,(struct a04_resume_observation *)(void *)scratch)==A04_CAPTURE_INVALID && !f.ncmd);
    return 0;
}
