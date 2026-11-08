/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Register model only: never proves external DMA writes drained. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "A04BR3RogueCoreResetHold.h"
struct model {uint32_t r[0x4000/4];unsigned resets,writes,waits,missing_reset_bit,stuck_irq;uint64_t read_mask;};
static uint32_t read32(void *p,uint32_t off){assert(off<0x4000);return ((struct model *)p)->r[off/4];}
static uint64_t read64(void *p,uint32_t off)
{struct model *m=p;uint64_t v=(uint64_t)read32(p,off+4)<<32|read32(p,off);if(off==0x100)v&=m->read_mask;if(off==0x100 && m->missing_reset_bit)v&=~UINT64_C(0x100000000);return v;}
static void write32(void *p,uint32_t off,uint32_t v)
{
 struct model *m=p;m->writes++;assert(off==0x8b0 || off==0xb40 || off==0xb30 || off==0xb48 || off==0xb38);
 m->r[off/4]=v;if(off==0x8b0 && !m->stuck_irq)m->r[0x8a8/4]=0;
}
static void write64(void *p,uint32_t off,uint64_t v)
{struct model *m=p;assert(off==0x100 && v==A04_RGX_CORE_RESET_ALL);m->resets++;m->r[off/4]=(uint32_t)v;m->r[off/4+1]=(uint32_t)(v>>32);}
static void waitus(void *p,uint32_t us){assert(us==10);((struct model *)p)->waits++;}
static void init(struct model *m){memset(m,0,sizeof(*m));m->read_mask=UINT64_MAX;m->r[0x3c8/4]=0x7c;m->r[0x3898/4]=0xff;}
int main(void)
{
 struct model m;struct a04_rgx_exec_io io={&m,read32,read64,write32,write64,waitus,NULL};struct a04_rgx_core_reset_observation out;
 init(&m);assert(a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,0,4,10,&out)==A04_EXEC_INPUT && !m.writes);
 init(&m);m.r[0x8a0/4]=1;assert(a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,1,4,10,&out)==A04_EXEC_INPUT && !m.writes);
 const unsigned busy[]={0x1358,0x1350,0x1320,0x1338,0x3870,0x3820};
 for(unsigned i=0;i<sizeof(busy)/sizeof(*busy);i++){
  init(&m);m.r[busy[i]/4]=1;
  assert(a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,1,4,10,&out)==A04_EXEC_TIMEOUT);
  assert(!m.resets && !out.reset_held && out.failed_register==busy[i] && m.waits==4);
 }
 init(&m);m.missing_reset_bit=1;
 assert(a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,1,4,10,&out)==A04_EXEC_TIMEOUT && m.resets==1 && !out.reset_held && out.failed_register==0x100);
 init(&m);m.stuck_irq=1;m.r[0x8a8/4]=1;
 assert(a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,1,4,10,&out)==A04_EXEC_TIMEOUT && !out.reset_held && out.failed_register==0x8a8);
 init(&m);assert(a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,1,4,10,&out)==A04_EXEC_OK);
 assert(out.phase==5 && out.reset_held && m.resets==1 && out.observed_reset==A04_RGX_CORE_RESET_ALL && !m.r[0x8a0/4]);
 /* Actual run111 reset readback contains implemented controls and leaves
  * the other family MASKFULL positions zero. It is not a failed reset. */
 init(&m);m.read_mask=UINT64_C(0x00000005ec000000);
 assert(a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,1,4,10,&out)==A04_EXEC_OK);
 assert(out.reset_held && out.observed_reset==m.read_mask && m.resets==1);
 const uint64_t anchors[]={UINT64_C(0x400000000),UINT64_C(0x100000000),UINT64_C(0x80000000),UINT64_C(0x20000000),UINT64_C(0x8000000)};
 for(unsigned i=0;i<sizeof(anchors)/sizeof(*anchors);i++){
  init(&m);m.read_mask=UINT64_C(0x00000005ec000000)&~anchors[i];
  assert(a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,1,4,10,&out)==A04_EXEC_TIMEOUT);
  assert(!out.reset_held && out.failed_register==0x100);
 }
 puts("GPU0 scoped reset-hold candidate: pre-reset busy prevents reset, implemented top-level reset controls required; absent family bits accepted, IRQ quiet, no reset deassert/DMA free/shared writes; MODEL ONLY PASS");
}
