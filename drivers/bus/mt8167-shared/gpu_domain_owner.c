/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "gpu_domain_owner.h"
#define PROTECT 0x24u
#define WAY (1u<<7)
#define DOMAINS ((1u<<25)|(1u<<24)|(1u<<4))
static int valid(const struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,uint64_t token)
{ return s && io && io->read && io->write && io->delay_us && s->held && s->lease==token && token && !s->poisoned && !s->revoked; }
static int bad(struct a04_gpu_domain_owner *s,int error,unsigned bank,unsigned off,uint32_t value)
{ s->poisoned=1;s->failed_bank=bank;s->failed_offset=off;s->failed_value=value;return error; }
static int readreg(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,unsigned bank,unsigned off,uint32_t *v)
{
 if(io->read(io->context,(enum a04_gpu_domain_bank)bank,off,v) || *v==UINT32_MAX)return bad(s,A04_GPU_DOMAIN_IO,bank,off,UINT32_MAX);
 return 0;
}
static int update(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,unsigned bank,unsigned off,uint32_t mask,uint32_t bits)
{
 uint32_t v;int e=readreg(s,io,bank,off,&v);if(e)return e;
 if(io->write(io->context,(enum a04_gpu_domain_bank)bank,off,(v&~mask)|(bits&mask)))return bad(s,A04_GPU_DOMAIN_IO,bank,off,v);
 return 0;
}
static int poll(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,unsigned bank,unsigned off,uint32_t mask,uint32_t want)
{
 uint32_t v=0;int e;
 for(unsigned i=0;i<10000;i++){
  e=readreg(s,io,bank,off,&v);if(e)return e;
  if((v&mask)==want)return 0;
  io->delay_us(io->context,10);
 }
 return bad(s,A04_GPU_DOMAIN_TIMEOUT,bank,off,v);
}
int a04_gpu_domain_acquire(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,uint64_t *token)
{
 uint32_t en,ack,way,v;int e;
 if(!s || !io || !io->read || !io->write || !io->delay_us || !token || s->poisoned || s->revoked)return A04_GPU_DOMAIN_INPUT;
 if(s->held || s->generation==UINT64_MAX)return A04_GPU_DOMAIN_BUSY;
 if((e=readreg(s,io,0,0x220,&en)) || (e=readreg(s,io,0,0x228,&ack)) || (e=readreg(s,io,0,0x204,&way)))return e;
 if(io->has_spm){
  /* Handoff prerequisite only: never rewrite the shared unlock register. */
  if((e=readreg(s,io,1,0,&v)))return e;
  if(!(v&1))return A04_GPU_DOMAIN_BUSY;
 }
 /* Never adopt firmware/another driver's already-protected domain as ours. */
 if((en&PROTECT) || (ack&PROTECT) || !(way&WAY)){
  /* Only a complete observed OFF state may be adopted on a fresh legitimate
   * parent assignment. A mere protection bit is not an ownership proof. */
  if(!io->has_spm || (en&PROTECT)!=PROTECT || (ack&PROTECT)!=PROTECT || (way&WAY))return A04_GPU_DOMAIN_BUSY;
  const unsigned off[3]={0x214,0x2c0,0x2c4};
  for(unsigned i=0;i<3;i++){
   if((e=readreg(s,io,1,off[i],&v)))return e;
   if((v&(i==2?0x1fu:0xff1fu))!=(i==2?0x12u:0xff12u))return A04_GPU_DOMAIN_BUSY;
  }
  if((e=readreg(s,io,1,0x60c,&v)))return e;
  if(v&DOMAINS)return A04_GPU_DOMAIN_BUSY;
  if((e=readreg(s,io,1,0x610,&v)))return e;
  if(v&DOMAINS)return A04_GPU_DOMAIN_BUSY;
  s->domains_off=s->protected_bus=1;
 }
 s->lease=++s->generation;s->held=1;*token=s->lease;return 0;
}
int a04_gpu_domain_protect(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,uint64_t token)
{
 int e;if(!valid(s,io,token))return A04_GPU_DOMAIN_INPUT;
 if(s->protected_bus || s->domains_off)return A04_GPU_DOMAIN_BUSY;
 /* Mark intent before write: timeout/partial failure must retain ownership. */
 s->protected_bus=1;
 if((e=update(s,io,0,0x220,PROTECT,PROTECT)))return e;
 return poll(s,io,0,0x228,PROTECT,PROTECT);
}
int a04_gpu_domain_unprotect(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,uint64_t token)
{
 int e;if(!valid(s,io,token))return A04_GPU_DOMAIN_INPUT;
 if(!s->protected_bus || s->domains_off)return A04_GPU_DOMAIN_BUSY;
 if((e=update(s,io,0,0x220,PROTECT,0)) || (e=poll(s,io,0,0x228,PROTECT,0)))return e;
 s->protected_bus=0;return 0;
}
static int domain_off(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,unsigned off,uint32_t status,int sram)
{
 int e;
 if(sram && ((e=update(s,io,1,off,0xf00,0xf00)) || (e=poll(s,io,1,off,0xf000,0xf000))))return e;
 if((e=update(s,io,1,off,2,2)) || (e=update(s,io,1,off,1,0)) ||
    (e=update(s,io,1,off,16,16)) || (e=update(s,io,1,off,4,0)) ||
    (e=update(s,io,1,off,8,0)) || (e=poll(s,io,1,0x60c,status,0)))return e;
 return poll(s,io,1,0x610,status,0);
}
int a04_gpu_domain_off(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,uint64_t token)
{
 int e;if(!valid(s,io,token))return A04_GPU_DOMAIN_INPUT;
 if(!io->has_spm)return A04_GPU_DOMAIN_UNASSIGNED; /* no writes on absent resource */
 if(s->domains_off || s->protected_bus)return A04_GPU_DOMAIN_BUSY;
 if((e=poll(s,io,1,0x60c,DOMAINS,DOMAINS)) || (e=poll(s,io,1,0x610,DOMAINS,DOMAINS)))return e;
 /* Linux MT8167 runtime put order: MFG, MFG_2D, MFG_ASYNC. */
 if((e=domain_off(s,io,0x214,1u<<4,1)) || (e=a04_gpu_domain_protect(s,io,token)) ||
    (e=domain_off(s,io,0x2c0,1u<<24,1)) || (e=update(s,io,0,0x204,WAY,0)) ||
    (e=domain_off(s,io,0x2c4,1u<<25,0)))return e;
 s->domains_off=1;return a04_gpu_domain_verify_off(s,io,token);
}
static int domain_on(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,unsigned off,uint32_t status,int sram)
{
 int e;
 if((e=update(s,io,1,off,4,4)) || (e=update(s,io,1,off,8,8)) ||
    (e=poll(s,io,1,0x60c,status,status)) || (e=poll(s,io,1,0x610,status,status)) ||
    (e=update(s,io,1,off,16,0)) || (e=update(s,io,1,off,2,0)) || (e=update(s,io,1,off,1,1)))return e;
 if(sram && ((e=update(s,io,1,off,0xf00,0)) || (e=poll(s,io,1,off,0xf000,0))))return e;
 return 0;
}
int a04_gpu_domain_on(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,uint64_t token)
{
 int e;if(!valid(s,io,token))return A04_GPU_DOMAIN_INPUT;
 if(!io->has_spm)return A04_GPU_DOMAIN_UNASSIGNED;
 if(!s->domains_off || !s->protected_bus)return A04_GPU_DOMAIN_BUSY;
 if((e=domain_on(s,io,0x2c4,1u<<25,0)) || (e=update(s,io,0,0x204,WAY,WAY)) ||
    (e=domain_on(s,io,0x2c0,1u<<24,1)))return e;
 /* Match source order: clear bus protection before enabling MFG. */
 if((e=update(s,io,0,0x220,PROTECT,0)) || (e=poll(s,io,0,0x228,PROTECT,0)) ||
    (e=domain_on(s,io,0x214,1u<<4,1)))return e;
 s->domains_off=0;s->protected_bus=0;return 0;
}
int a04_gpu_domain_release(struct a04_gpu_domain_owner *s,uint64_t token)
{
 if(!s || !s->held || s->lease!=token || !token || s->poisoned || s->revoked)return A04_GPU_DOMAIN_INPUT;
 if(s->protected_bus || s->domains_off)return A04_GPU_DOMAIN_BUSY;
 s->held=0;s->lease=0;return 0;
}
void a04_gpu_domain_revoke(struct a04_gpu_domain_owner *s)
{ if(s){s->revoked=1;if(s->held)s->poisoned=1;} }

int a04_gpu_domain_verify_off(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,uint64_t token)
{
 uint32_t v;int e;const unsigned offsets[3]={0x214,0x2c0,0x2c4};
 if(!valid(s,io,token))return A04_GPU_DOMAIN_INPUT;
 if(!io->has_spm)return A04_GPU_DOMAIN_UNASSIGNED;
 if(!s->domains_off || !s->protected_bus)return A04_GPU_DOMAIN_BUSY;
 for(unsigned i=0;i<3;i++){
  if((e=readreg(s,io,1,offsets[i],&v)))return e;
  if((v&(i==2?0x1fu:0xff1fu))!=(i==2?0x12u:0xff12u))return bad(s,A04_GPU_DOMAIN_IO,1,offsets[i],v);
 }
 const unsigned spm_status[2]={0x60c,0x610};
 for(unsigned i=0;i<2;i++){
  if((e=readreg(s,io,1,spm_status[i],&v)))return e;
  if(v&DOMAINS)return bad(s,A04_GPU_DOMAIN_IO,1,spm_status[i],v);
 }
 const unsigned infra_off[3]={0x220,0x228,0x204};
 for(unsigned i=0;i<3;i++){
  if((e=readreg(s,io,0,infra_off[i],&v)))return e;
  if((v&(i==2?WAY:PROTECT))!=(i==2?0:PROTECT))return bad(s,A04_GPU_DOMAIN_IO,0,infra_off[i],v);
 }
 return 0;
}
int a04_gpu_domain_release_off(struct a04_gpu_domain_owner *s,const struct a04_gpu_domain_io *io,uint64_t token)
{
 int e=a04_gpu_domain_verify_off(s,io,token);if(e)return e;
 s->held=0;s->lease=0;return 0; /* Keep domains OFF/protected, never restart stale FW. */
}
