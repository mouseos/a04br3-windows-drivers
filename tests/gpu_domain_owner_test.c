/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Explicit MT8167 register model. Not GPU hardware or a DMA-stop proof. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "gpu_domain_owner.h"
struct model {uint32_t infra[0x1000/4],spm[0x1000/4];unsigned writes,reads,waits,fail_write,no_ack,spm_access;};
static unsigned status(unsigned off){return off==0x214?1u<<4:off==0x2c0?1u<<24:1u<<25;}
static int read32(void *ctx,enum a04_gpu_domain_bank bank,unsigned off,uint32_t *v)
{
 struct model *m=ctx;m->reads++;
 assert(off<0x1000 && !(off&3));if(bank)m->spm_access++;
 *v=bank?m->spm[off/4]:m->infra[off/4];return 0;
}
static int write32(void *ctx,enum a04_gpu_domain_bank bank,unsigned off,uint32_t v)
{
 struct model *m=ctx;uint32_t *r=bank?m->spm:m->infra;unsigned mask;
 assert(off<0x1000 && !(off&3));m->writes++;if(bank)m->spm_access++;
 if(m->writes==m->fail_write)return -1;
 if(bank){assert(off==0x214 || off==0x2c0 || off==0x2c4);mask=off==0x2c4?0x1f:0xf1f;}
 else {assert(off==0x220 || off==0x204);mask=off==0x220?0x24:0x80;}
 assert(!((r[off/4]^v)&~mask)); /* no CPU4/watchdog/other consumer bits */
 r[off/4]=v;
 if(!m->no_ack){
  if(!bank && off==0x220)m->infra[0x228/4]=(m->infra[0x228/4]&~0x24)|(v&0x24);
  if(bank){
   if(off!=0x2c4)r[off/4]=(r[off/4]&~0xf000)|((v&0xf00)<<4);
   unsigned b=status(off);if((v&12)==12){r[0x60c/4]|=b;r[0x610/4]|=b;}
   if(!(v&12)){r[0x60c/4]&=~b;r[0x610/4]&=~b;}
  }
 }
 return 0;
}
static void delay(void *ctx,unsigned us){assert(us==10);((struct model *)ctx)->waits++;}
static void init(struct model *m)
{
 memset(m,0,sizeof(*m));m->spm[0]=1;m->infra[0x220/4]=0x81000000;m->infra[0x228/4]=0x81000000;
 m->infra[0x204/4]=0xa0000080;
 m->spm[0x214/4]=0x4000000d;m->spm[0x2c0/4]=0x4000000d;m->spm[0x2c4/4]=0x4000000d;
 m->spm[0x60c/4]=m->spm[0x610/4]=(1u<<25)|(1u<<24)|(1u<<4)|0x2000;
}
int main(void)
{
 struct model m;struct a04_gpu_domain_owner s={0};struct a04_gpu_domain_io io={&m,read32,write32,delay,0};uint64_t token=0;
 init(&m);assert(!a04_gpu_domain_acquire(&s,&io,&token) && token==1 && !m.writes);
 assert(a04_gpu_domain_off(&s,&io,token)==A04_GPU_DOMAIN_UNASSIGNED && !m.writes && !m.spm_access);
 assert(!a04_gpu_domain_protect(&s,&io,token));assert(m.infra[0x220/4]==0x81000024);
 assert(a04_gpu_domain_release(&s,token)==A04_GPU_DOMAIN_BUSY);
 assert(!a04_gpu_domain_unprotect(&s,&io,token) && !m.spm_access);
 assert(!a04_gpu_domain_release(&s,token));assert(!a04_gpu_domain_acquire(&s,&io,&token) && token==2);
 unsigned before=m.writes;assert(a04_gpu_domain_protect(&s,&io,1)==A04_GPU_DOMAIN_INPUT && m.writes==before);
 assert(!a04_gpu_domain_release(&s,token));
 /* Full module tested only with explicit assigned-SPM model. Windows
  * ecaa1014 binding does not advertise this profile or map SPM. */
 memset(&s,0,sizeof(s));init(&m);io.has_spm=1;
 assert(!a04_gpu_domain_acquire(&s,&io,&token));assert(!a04_gpu_domain_off(&s,&io,token));
 assert(s.domains_off && s.protected_bus && m.spm[0x60c/4]==0x2000 && m.spm[0x610/4]==0x2000);
 assert(a04_gpu_domain_unprotect(&s,&io,token)==A04_GPU_DOMAIN_BUSY);
 assert(!a04_gpu_domain_on(&s,&io,token));assert(m.spm[0x60c/4]==((1u<<25)|(1u<<24)|(1u<<4)|0x2000));
 assert(!a04_gpu_domain_release(&s,token));unsigned full_writes=m.writes;
 /* Every write failure is terminal, retains owner and cannot be released. */
 for(unsigned fail=1;fail<=full_writes;fail++){
  memset(&s,0,sizeof(s));init(&m);m.fail_write=fail;
  assert(!a04_gpu_domain_acquire(&s,&io,&token));int e=a04_gpu_domain_off(&s,&io,token);
  if(!e)e=a04_gpu_domain_on(&s,&io,token);
  assert(e==A04_GPU_DOMAIN_IO && s.poisoned && s.held);
  assert(a04_gpu_domain_release(&s,token)==A04_GPU_DOMAIN_INPUT);
 }
 memset(&s,0,sizeof(s));init(&m);io.has_spm=1;m.spm[0]=0;
 assert(a04_gpu_domain_acquire(&s,&io,&token)==A04_GPU_DOMAIN_BUSY && !m.writes && !s.held);
 memset(&s,0,sizeof(s));init(&m);assert(!a04_gpu_domain_acquire(&s,&io,&token));
 assert(!a04_gpu_domain_off(&s,&io,token));unsigned offwrites=m.writes;
 assert(!a04_gpu_domain_verify_off(&s,&io,token));assert(!a04_gpu_domain_release_off(&s,&io,token) && !s.held && m.writes==offwrites);
 memset(&s,0,sizeof(s)); /* fresh legitimate parent assignment, hardware remains fully OFF */
 assert(!a04_gpu_domain_acquire(&s,&io,&token) && s.domains_off && s.protected_bus && m.writes==offwrites);
 assert(!a04_gpu_domain_on(&s,&io,token));assert(!a04_gpu_domain_release(&s,token));
 memset(&s,0,sizeof(s));init(&m);io.has_spm=0;m.no_ack=1;
 assert(!a04_gpu_domain_acquire(&s,&io,&token));assert(a04_gpu_domain_protect(&s,&io,token)==A04_GPU_DOMAIN_TIMEOUT);
 assert(m.waits==10000 && s.poisoned && s.held && !m.spm_access);
 memset(&s,0,sizeof(s));init(&m);m.infra[0x220/4]=UINT32_MAX;
 assert(a04_gpu_domain_acquire(&s,&io,&token)==A04_GPU_DOMAIN_IO && s.poisoned && !m.writes);
 memset(&s,0,sizeof(s));init(&m);assert(!a04_gpu_domain_acquire(&s,&io,&token));
 a04_gpu_domain_revoke(&s);before=m.writes;
 assert(a04_gpu_domain_protect(&s,&io,token)==A04_GPU_DOMAIN_INPUT && m.writes==before && s.held);
 puts("MT8167 GPU parent domain: INFRA-only no SPM/core access; modeled off/on, all write failures, bounded ack timeout, stale epoch, revoke and unrelated-bit preservation PASS; no hardware proof");
}
