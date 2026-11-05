/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "parent_resources.h"
int a04_parent_validate_resources(const struct a04_parent_resource *r,size_t n,
                                  struct a04_parent_mapping *out)
{
    struct a04_parent_mapping tmp={0};size_t i;
    if(!r||!out||(n!=4 && n!=5))return -1;
    for(i=0;i<n;++i) {
        if(r[i].length!=0x1000u || r[i].exclusive!=1)return -1;
        if(r[i].base==0x10000000u && !tmp.topck)tmp.topck=r[i].base;
        else if(r[i].base==0x10001000u && !tmp.infra)tmp.infra=r[i].base;
        else if(r[i].base==0x10005000u && !tmp.gpio)tmp.gpio=r[i].base;
        else if(r[i].base==0x1000f000u && !tmp.pwrap)tmp.pwrap=r[i].base;
        else if(r[i].base==0x10018000u && !tmp.apmixed)tmp.apmixed=r[i].base;
        else return -1;
    }
    if(!tmp.topck||!tmp.infra||!tmp.gpio||!tmp.pwrap)return -1;
    *out=tmp;return 0;
}
