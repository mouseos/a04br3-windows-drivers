/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "cpu_observer.h"
struct fake { unsigned calls, fail_at, change_at; };
static int read_field(void *ctx, enum a04_cpu_field field, uint32_t *out) {
 static const uint32_t values[]={4,1,0xc8000,1,0,1,96,80};
 struct fake *f=ctx; unsigned n=f->calls++;
 assert((unsigned)field==n%A04_CPU_FIELD_COUNT);
 if (n==f->fail_at) return -1;
 *out=values[field]+(n==f->change_at); return 0;
}
int main(void) {
 struct a04_cpu_snapshot a={4,1,0xc8000,1,0,1,96,80},b=a;
 struct a04_cpu_observation o;
 struct fake f={0,99,99};
 assert(!a04_cpu_capture(read_field,&f,1,0,&o) && f.calls==16 && o.valid==7);
 f=(struct fake){0,5,99}; assert(a04_cpu_capture(read_field,&f,1,0,&o)<0 && f.calls==6 && !o.valid);
 f=(struct fake){0,99,10}; assert(a04_cpu_capture(read_field,&f,1,0,&o)<0 && f.calls==16 && !o.valid);
 assert(a04_cpu_capture(0,0,1,0,&o)<0 && !o.valid);
 assert(a04_cpu_observe(&a,&b,1,0,&o)==0 && o.valid==7 && o.devinfo_bin==0);
 assert(o.configured_pll_hz==1300000000u && o.vproc_uv==1300000u);
 a.vproc_ctrl=b.vproc_ctrl=2; assert(!a04_cpu_observe(&a,&b,0,0,&o));
 assert(o.valid==3 && o.vproc_uv==1200000u);
 b.pll1++; assert(a04_cpu_observe(&a,&b,1,0,&o)<0 && o.valid==0); b=a;
 a.pll_power=b.pll_power=3; assert(!a04_cpu_observe(&a,&b,1,0xffffffffu,&o));
 assert(o.valid==A04_CPU_VPROC_VALID && !o.configured_pll_hz);
 a.pll_power=b.pll_power=1; a.vproc_en=b.vproc_en=0;
 assert(!a04_cpu_observe(&a,&b,1,3,&o) && o.valid==A04_CPU_PLL_VALID);
 a.mux=b.mux=0; assert(!a04_cpu_observe(&a,&b,1,1,&o) && o.valid==A04_CPU_BIN_VALID && o.devinfo_bin==1);
 assert(a04_cpu_observe(0,&b,0,0,&o)<0);
 return 0;
}
