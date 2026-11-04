/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_execution.h"
struct fake { unsigned call, fail; char log[16]; };
static int step(struct fake *f,char c) { f->log[f->call++]=c; return f->call==f->fail; }
#define CB(name,ch) static int name(void *p){return step(p,ch);}
CB(syncdev,'s') CB(map,'m') CB(kick,'k') CB(geom,'g') CB(frag,'f')
CB(synccpu,'c') CB(clean,'x') CB(unmap,'u')
int main(void)
{
    struct a04_render_owned_range ranges[]={{0x8000000000ULL,0x100000,4096,5},
                                              {0x8000001000ULL,0x101000,4096,3}};
    struct fake f={0}; struct a04_render_execution s={0};
    struct a04_render_execution_ops o={&f,syncdev,map,kick,geom,frag,synccpu,clean,unmap};
    assert(!a04_render_validate_owned_ranges(ranges,2));
    assert(!a04_render_execute_owned(&s,ranges,2,&o));
    assert(s.state==A04_RENDER_RETIRED && !memcmp(f.log,"smkgfcxu",8));
    assert(a04_render_execute_owned(&s,ranges,2,&o));
    for (unsigned fail=1;fail<=8;++fail) {
        memset(&f,0,sizeof(f));f.fail=fail;s.state=A04_RENDER_IDLE;
        assert(a04_render_execute_owned(&s,ranges,2,&o));
        assert(f.call==fail);
        assert(s.state==(fail==1 ? A04_RENDER_IDLE : A04_RENDER_POISONED));
    }
    memset(&f,0,sizeof(f)); s.state=A04_RENDER_IDLE;
    o.firmware_cleanup=0;
    assert(a04_render_execute_owned(&s,ranges,2,&o) && !f.call && s.state==A04_RENDER_IDLE);
    o.firmware_cleanup=clean;
    assert(a04_render_validate_owned_ranges(0,1));
    assert(a04_render_validate_owned_ranges(ranges,0));
    assert(a04_render_validate_owned_ranges(ranges,4097));
    ranges[1].gpu_va=ranges[0].gpu_va; assert(a04_render_validate_owned_ranges(ranges,2));
    ranges[1].gpu_va=0x8000001000ULL; ranges[1].dma=ranges[0].dma;
    assert(a04_render_validate_owned_ranges(ranges,2));
    ranges[1].dma=0x101000; ranges[1].access=7; assert(a04_render_validate_owned_ranges(ranges,2));
    ranges[1].access=3; ranges[1].dma=0xfffff000u; ranges[1].size=8192;
    assert(a04_render_validate_owned_ranges(ranges,2));
    return 0;
}
