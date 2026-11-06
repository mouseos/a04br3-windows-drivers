/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/shared_frame_plan.h"
int main(void)
{
    struct a04_shared_tile t;struct a04_tq_ring_plan p;uint32_t off=0,pads=0;
    for(uint32_t i=0;i<1000;i++){
        assert(a04_shared_tile_at(i,&t));assert(t.x==(i%25)*32 && t.y==(i/25)*32);
        assert(t.byte_offset%128==0 && t.last_byte<=4096000);
        assert(a04_tq_ring_plan(off,off,off,&p));if(p.pad_bytes){pads++;assert(p.pad_bytes>=40 && p.pad_at==off);}
        off=p.next;
    }
    assert(pads>0 && t.x==768 && t.y==1248 && t.last_byte==4096000);
    assert(!a04_shared_tile_at(1000,&t));assert(!a04_tq_ring_plan(0,8,0,&p));
    assert(!a04_tq_ring_plan(0,0,8,&p));
    assert(!a04_tq_ring_plan(4080,4080,4080,&p));assert(!a04_tq_ring_plan(1,1,1,&p));
    return 0;
}
