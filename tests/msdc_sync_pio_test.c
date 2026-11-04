/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "sync_pio.h"
#include <assert.h>
#include <string.h>
struct fake { uint32_t regs[64], current, ints, commands[4], argc[4], count, bytes, total, fail_data, fail_stop, inactive, fifo_stuck, no_data, ticks; };
static uint32_t read32(void *ctx,uint32_t off)
{
    struct fake *f=ctx;
    if(off==0x00c) return f->ints | ((f->current==18 || f->current==17) && f->bytes==f->total ? 0x1000u:0) | ((f->current==18 || f->current==17) && f->fail_data && f->bytes>=64 ? 0x8000u:0);
    if(off==0x014) return f->fifo_stuck ? (1u<<31) : ((f->current==18 || f->current==17) && f->bytes<f->total && !f->no_data ? 64u:0u);
    if(off==0x01c) { uint32_t v=0; int i; for(i=0;i<4;i++) v|=((f->bytes++)&255u)<<(8*i); return v; }
    if(off==0x09c) return f->inactive?0:1;
    if(off==0x008) return 1u<<16;
    return f->regs[off/4];
}
static uint8_t read8(void *ctx,uint32_t off) { struct fake *f=ctx; assert(off==0x01c); return (uint8_t)f->bytes++; }
static void write32(void *ctx,uint32_t off,uint32_t val)
{
    struct fake *f=ctx;
    assert(off!=0x090 && off!=0x098 && off!=0x09c && off!=0x0a8);
    if(off==0x00c) f->ints &= ~val;
    else if(off==0x014) { assert(val&(1u<<31)); }
    else if(off==0x034) {
        assert(f->count<4); f->current=val&63; f->commands[f->count]=val; f->argc[f->count++]=f->regs[0x038/4];
        f->ints=(f->current==12 && f->fail_stop) ? 0x400u : 0x100u;
    } else { f->regs[off/4]=val; if(off==0x050) f->total=val*512u; }
}
static int is_cancelled(void *ctx) { struct fake *f=ctx; return f->bytes>=64; }
static uint64_t now(void *ctx) { struct fake *f=ctx; return f->ticks++; }
static void init(struct fake *f) { memset(f,0,sizeof(*f));f->regs[0]=9;f->inactive=1; }
int main(void)
{
    struct fake f; struct a04_msdc_io io={&f,read32,read8,write32}; uint8_t tmp[4096],out[4096]; unsigned i;
    init(&f);memset(out,0xcc,sizeof(out));
    assert(a04_sync_pio_read(&io,123,8,tmp,sizeof(tmp),out,sizeof(out),200,0)==A04_SYNC_OK);
    assert(f.count==2 && (f.commands[0]&63)==23 && f.argc[0]==8 && (f.commands[1]&63)==18 && f.argc[1]==123);
    for(i=0;i<sizeof(out);++i) assert(out[i]==(uint8_t)i);
    init(&f);memset(out,0xcc,sizeof(out));f.fail_data=1;
    assert(a04_sync_pio_read(&io,1,8,tmp,sizeof(tmp),out,sizeof(out),200,0)==A04_SYNC_DATA_ERROR);
    assert(f.count==3 && (f.commands[2]&63)==12);for(i=0;i<sizeof(out);++i) assert(out[i]==0xcc);
    init(&f);f.fail_data=1;f.fail_stop=1;
    assert(a04_sync_pio_read(&io,1,8,tmp,sizeof(tmp),out,sizeof(out),200,0)==A04_SYNC_STOP_ERROR);
    init(&f);f.inactive=0;assert(a04_sync_pio_read(&io,1,8,tmp,sizeof(tmp),out,sizeof(out),200,0)==A04_SYNC_BUSY && !f.count);
    init(&f);f.regs[0x010/4]=1u<<19;assert(a04_sync_pio_read(&io,1,8,tmp,sizeof(tmp),out,sizeof(out),200,0)==A04_SYNC_BUSY && !f.count);
    init(&f);assert(a04_sync_pio_read(&io,0xffffffffu,2,tmp,sizeof(tmp),out,sizeof(out),200,0)==A04_SYNC_INVALID && !f.count);
    assert(a04_sync_pio_read(&io,1,2,tmp,sizeof(tmp),tmp+1,sizeof(tmp)-1,200,0)==A04_SYNC_INVALID && !f.count);
    init(&f);f.fifo_stuck=1;assert(a04_sync_pio_read(&io,1,8,tmp,sizeof(tmp),out,sizeof(out),10,0)==A04_SYNC_TIMEOUT && f.count==1);
    init(&f); assert(a04_sync_pio_read(&io,3,1,tmp,sizeof(tmp),out,sizeof(out),200,0)==A04_SYNC_OK);
    assert(f.count==1 && (f.commands[0]&63)==17);
    init(&f); f.no_data=1; memset(out,0xcc,sizeof(out));
    { struct a04_sync_clock clock={&f,now,3,0};
      assert(a04_sync_pio_read(&io,1,8,tmp,sizeof(tmp),out,sizeof(out),1000000,&clock)==A04_SYNC_TIMEOUT);
      assert(f.ticks<20 && f.count==3 && (f.commands[2]&63)==12);
      for(i=0;i<sizeof(out);++i) assert(out[i]==0xcc); }
    init(&f); memset(out,0xcc,sizeof(out));
    { struct a04_sync_clock clock={&f,now,100,is_cancelled};
      assert(a04_sync_pio_read(&io,1,8,tmp,sizeof(tmp),out,sizeof(out),200,&clock)==A04_SYNC_CANCELLED);
      assert(f.count==2 && f.bytes==64);
      for(i=0;i<sizeof(out);++i) assert(out[i]==0xcc); }
    return 0;
}
