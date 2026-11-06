/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/wddm-display/mt8167_cmdq_packet.h"
/* Minimal execution model: a stale event must not allow the L0 write; only
 * a subsequent event advances the no-clear wait. This models, not measures,
 * the hardware's event semantics. */
static unsigned run_until_wait(const uint32_t *w,unsigned pc,int *event,uint32_t *l0)
{
    while(pc<10) {
        uint32_t value=w[pc],op=w[pc+1];
        if((op>>24)==0x20 && value==0x80000000u)*event=0;
        else if((op>>24)==0x20 && value==0x8001u){if(!*event)return pc;}
        else if(op==0x04017f40u)*l0=value;
        pc+=2;
    }
    return pc;
}
static unsigned run_pair(const uint32_t *w,unsigned pc,int *event,uint32_t *l0,uint32_t *l3,int *eoc)
{
    while(pc<16) {
        uint32_t value=w[pc],op=w[pc+1];
        if((op>>24)==0x20 && value==0x80000000u)*event=0;
        else if((op>>24)==0x20 && value==0x8001u){if(!*event)return pc;}
        else if(op==0x04017f40u)*l0=value;
        else if(op==0x04017fa0u)*l3=value;
        else if(op==0x40000000u)*eoc=1;
        pc+=2;
    }
    return pc;
}
int main(void)
{
    uint32_t words[10]={0},restore[10]={0};
    assert(!a04_mt8167_cmdq_l0_address(words,9,27,0x80001000,1));
    assert(!a04_mt8167_cmdq_l0_address(words,10,77,0x80001000,1));
    assert(!a04_mt8167_cmdq_l0_address(words,10,27,0x80001000,0));
    assert(!a04_mt8167_cmdq_l0_address(words,10,27,0,1));
    assert(!a04_mt8167_cmdq_l0_address(words,10,27,0x80001001,1));
    for(unsigned i=0;i<10;i++)assert(!words[i]);
    assert(a04_mt8167_cmdq_l0_address(words,10,27,0xb8c1b000,1));
    assert(words[0]==0x80000000 && words[1]==0x2000001b);
    assert(words[2]==0x8001 && words[3]==words[1]);
    assert(words[4]==0xb8c1b000 && words[5]==0x04017f40);
    assert(words[6]==1 && words[7]==0x40000000 && words[8]==8 && words[9]==0x10000000);
    { int event=1;uint32_t l0=0x90000000;unsigned pc=run_until_wait(words,0,&event,&l0);
      assert(pc==2 && !event && l0==0x90000000);event=1;
      assert(run_until_wait(words,pc,&event,&l0)==10 && l0==0xb8c1b000); }
    assert(a04_mt8167_cmdq_l0_address(restore,10,38,0x90000000,1));
    assert(restore[1]==0x20000026 && restore[4]==0x90000000 && restore[5]==words[5]);
    assert(!a04_mt8167_cmdq_finalize(restore,10,16));
    assert(!a04_mt8167_cmdq_finalize(restore,10,12));
    assert(!a04_mt8167_cmdq_finalize(restore,10,15));
    assert(!a04_mt8167_cmdq_finalize(restore,10,3));
    assert(a04_mt8167_cmdq_finalize(restore,10,0));
    assert(restore[8]==0x1020ac00 && restore[9]==0x10000001);
    assert(!a04_mt8167_cmdq_finalize(restore,10,0));
    {
      uint32_t pair[12]={0},saved[12]={0};
      assert(!a04_mt8167_cmdq_layer_addresses(pair,12,38,0x90000000,0,1));
      assert(!a04_mt8167_cmdq_layer_addresses(pair,11,38,0x90000000,0x91000000,1));
      assert(!a04_mt8167_cmdq_layer_addresses(pair,12,38,0x90000000,0x91000000,0));
      assert(a04_mt8167_cmdq_layer_addresses(pair,12,38,0xb8c1b000,0x91000000,1));
      assert(pair[0]==0x80000000 && pair[1]==0x20000026 && pair[2]==0x8001 && pair[3]==pair[1]);
      assert(pair[5]==0x04017f40 && pair[7]==0x04017fa0);
      assert(pair[8]==1 && pair[9]==0x40000000 && pair[10]==8 && pair[11]==0x10000000);
      assert(a04_mt8167_cmdq_layer_addresses(saved,12,38,0x90000000,0x91000000,1));
      assert(saved[4]==0x90000000 && saved[6]==0x91000000);
      assert(a04_mt8167_cmdq_finalize(saved,12,0));
      assert(saved[10]==0x1020ac00 && saved[11]==0x10000001);
    }
    {
      uint32_t frame[16]={0},l0=0x1000,l3=0x2000;int event=1,eoc=0;unsigned pc;
      assert(!a04_mt8167_cmdq_layer_frame(frame,15,27,0x3000,0x4000,1));
      assert(a04_mt8167_cmdq_layer_frame(frame,16,27,0x3000,0x4000,1));
      pc=run_pair(frame,0,&event,&l0,&l3,&eoc);
      assert(pc==2 && l0==0x1000 && l3==0x2000 && !eoc);
      event=1;pc=run_pair(frame,pc,&event,&l0,&l3,&eoc);
      assert(pc==10 && l0==0x3000 && l3==0x4000 && !event && !eoc);
      event=1;assert(run_pair(frame,pc,&event,&l0,&l3,&eoc)==16 && eoc);
      assert(a04_mt8167_cmdq_finalize(frame,16,0));
      assert(frame[14]==0x1020ac00 && frame[15]==0x10000001);
    }
    return 0;
}
