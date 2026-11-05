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
