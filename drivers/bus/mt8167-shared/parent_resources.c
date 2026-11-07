/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "parent_resources.h"
int a04_parent_validate_resources(const struct a04_parent_resource *r,size_t n,
                                  struct a04_parent_mapping *out)
{
    struct a04_parent_mapping tmp={0};size_t i;
    if(!r||!out||n!=4)return -1;
    for(i=0;i<n;++i) {
        if(r[i].length!=0x1000u || r[i].exclusive!=1)return -1;
        if(r[i].base==0x10000000u && !tmp.topck)tmp.topck=r[i].base;
        else if(r[i].base==0x10001000u && !tmp.infra)tmp.infra=r[i].base;
        else if(r[i].base==0x10005000u && !tmp.gpio)tmp.gpio=r[i].base;
        else if(r[i].base==0x1000f000u && !tmp.pwrap)tmp.pwrap=r[i].base;
        else return -1;
    }
    if(!tmp.topck||!tmp.infra||!tmp.gpio||!tmp.pwrap)return -1;
    *out=tmp;return 0;
}

/* Raw list entries identify board devices; paired translated entries provide
 * physical addresses. Reject overlapping mappings and preserve out on error. */
int a04_parent_translate_resources(const struct a04_parent_resource *raw,
    const struct a04_parent_resource *translated,size_t n,struct a04_parent_mapping *out)
{
    struct a04_parent_mapping rawmap,tmp={0};size_t i,j;
    if(!out || !translated || a04_parent_validate_resources(raw,n,&rawmap))return -1;
    for(i=0;i<n;++i) {
        uint64_t base=translated[i].base;
        if(!base || base>UINT64_MAX-0xfffu || translated[i].length!=0x1000u || translated[i].exclusive!=1)return -1;
        for(j=0;j<i;++j) {
            uint64_t other=translated[j].base;
            if((base<=other ? other-base : base-other)<0x1000u)return -1;
        }
        if(raw[i].base==rawmap.topck)tmp.topck=base;
        else if(raw[i].base==rawmap.infra)tmp.infra=base;
        else if(raw[i].base==rawmap.gpio)tmp.gpio=base;
        else if(raw[i].base==rawmap.pwrap)tmp.pwrap=base;
        else return -1;
    }
    *out=tmp;return 0;
}

int a04_parent_translate_audio_resources(const struct a04_parent_resource *raw,
    const struct a04_parent_resource *translated,size_t n,
    struct a04_parent_mapping *out,uint64_t *apmixed)
{
    struct a04_parent_resource r[4],t[4];struct a04_parent_mapping m;
    uint64_t a=0;size_t i,j,k=0;
    if(!raw || !translated || !out || !apmixed || n!=5)return -1;
    for(i=0;i<n;i++) {
        uint64_t base=translated[i].base;uint32_t len=translated[i].length;
        if(!base || !len || base>UINT64_MAX-(len-1) || translated[i].exclusive!=1)return -1;
        for(j=0;j<i;j++) {
            uint64_t other=translated[j].base;uint32_t otherlen=translated[j].length;
            if(base<=other ? other-base<len : base-other<otherlen)return -1;
        }
        if(raw[i].base==0x10018000u) {
            if(a || (raw[i].length!=0x710u && raw[i].length!=0x714u) || raw[i].exclusive!=1 || len!=raw[i].length)return -1;
            a=base;
        } else {
            if(k==4)return -1;
            r[k]=raw[i];t[k]=translated[i];k++;
        }
    }
    if(k!=4 || !a || a04_parent_translate_resources(r,t,4,&m))return -1;
    *out=m;*apmixed=a;return 0;
}

int a04_parent_translate_gpu_domain_resources(const struct a04_parent_resource *raw,
 const struct a04_parent_resource *translated,size_t count,struct a04_parent_mapping *out,
 uint64_t *apmixed,uint64_t *spm)
{
 struct a04_parent_resource r[5],t[5];struct a04_parent_mapping map;
 uint64_t s=0,a=0;size_t k=0;
 if(!raw || !translated || !out || !apmixed || !spm || count!=6)return -1;
 for(size_t i=0;i<count;i++){
  uint64_t base=translated[i].base;uint32_t bytes=translated[i].length;
  if(!base || !bytes || base>UINT64_MAX-(bytes-1) || translated[i].exclusive!=1)return -1;
  for(size_t j=0;j<i;j++){
   uint64_t other=translated[j].base;uint32_t len=translated[j].length;
   if(base<=other ? other-base<bytes : base-other<len)return -1;
  }
  if(raw[i].base==0x10006000u){
   if(s || raw[i].length!=0x1000 || raw[i].exclusive!=1 || bytes!=0x1000)return -1;
   s=base;
  }else{
   if(k==5)return -1;
   r[k]=raw[i];t[k]=translated[i];k++;
  }
 }
 if(k!=5 || !s || a04_parent_translate_audio_resources(r,t,k,&map,&a))return -1;
 *out=map;*apmixed=a;*spm=s;return 0;
}
