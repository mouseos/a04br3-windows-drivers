/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "parent_resources.h"
int main(void)
{
 struct a04_parent_resource raw[6]={{0x10000000,0x1000,1},{0x10001000,0x1000,1},
  {0x10005000,0x1000,1},{0x1000f000,0x1000,1},{0x10018000,0x714,1},{0x10006000,0x1000,1}},t[6];
 struct a04_parent_mapping m,old;uint64_t ap,sp;memcpy(t,raw,sizeof(t));memset(&old,0xa5,sizeof(old));
 m=old;ap=sp=77;
 assert(a04_parent_translate_gpu_domain_resources(raw,t,5,&m,&ap,&sp) && !memcmp(&m,&old,sizeof(m)) && ap==77 && sp==77);
 assert(!a04_parent_translate_gpu_domain_resources(raw,t,6,&m,&ap,&sp) && ap==0x10018000 && sp==0x10006000 && m.infra==0x10001000);
 for(unsigned i=0;i<4;i++){
  memcpy(t,raw,sizeof(t));m=old;ap=sp=77;
  if(i==0)t[5].base=t[0].base;
  if(i==1)t[5].exclusive=0;
  if(i==2)t[5].length=0x800;
  if(i==3){t[5].base=UINT64_MAX-5;}
  assert(a04_parent_translate_gpu_domain_resources(raw,t,6,&m,&ap,&sp) && !memcmp(&m,&old,sizeof(m)) && ap==77 && sp==77);
 }
 memcpy(t,raw,sizeof(t));t[5].base=0x20006000;
 assert(!a04_parent_translate_gpu_domain_resources(raw,t,6,&m,&ap,&sp) && sp==0x20006000);
 puts("GPU SPM parent resources: explicit sixth exclusive window only; no raw fallback/overlap/short range and unchanged error outputs PASS");
}
