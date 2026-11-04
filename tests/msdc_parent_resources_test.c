/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "parent_resources.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    struct a04_parent_resource r[4]={{0x10005000u,0x1000,1},{0x1000f000u,0x1000,1},{0x10000000u,0x1000,1},{0x10001000u,0x1000,1}};
    struct a04_parent_mapping m={1,2,3,4},old=m;
    assert(a04_parent_validate_resources(r,3,&m)!=0 && !memcmp(&m,&old,sizeof m));
    assert(!a04_parent_validate_resources(r,4,&m) && m.topck==0x10000000 && m.infra==0x10001000 && m.gpio==0x10005000 && m.pwrap==0x1000f000);
    old=m;r[1].exclusive=0;assert(a04_parent_validate_resources(r,4,&m)!=0 && !memcmp(&m,&old,sizeof m));
    r[1].exclusive=1;r[1].base=r[0].base;assert(a04_parent_validate_resources(r,4,&m)!=0 && !memcmp(&m,&old,sizeof m));
    r[1].base=0x1000f000;r[1].length=0x2000;assert(a04_parent_validate_resources(r,4,&m)!=0);
    r[1].length=0x1000;r[1].base=0x11000f000ULL;assert(a04_parent_validate_resources(r,4,&m)!=0);
    {
        struct a04_parent_resource raw[4]={{0x10005000u,0x1000,1},{0x1000f000u,0x1000,1},{0x10000000u,0x1000,1},{0x10001000u,0x1000,1}};
        struct a04_parent_resource tr[4]={{0x20005000u,0x1000,1},{0x2000f000u,0x1000,1},{0x20000000u,0x1000,1},{0x20001000u,0x1000,1}};
        assert(!a04_parent_translate_resources(raw,tr,4,&m));
        assert(m.topck==0x20000000 && m.infra==0x20001000 && m.gpio==0x20005000 && m.pwrap==0x2000f000);
        old=m;tr[1].base=tr[0].base+1;
        assert(a04_parent_translate_resources(raw,tr,4,&m)!=0 && !memcmp(&m,&old,sizeof m));
        tr[1].base=UINT64_MAX-0xffe;
        assert(a04_parent_translate_resources(raw,tr,4,&m)!=0 && !memcmp(&m,&old,sizeof m));
        tr[1].base=0x2000f000;tr[1].exclusive=0;
        assert(a04_parent_translate_resources(raw,tr,4,&m)!=0 && !memcmp(&m,&old,sizeof m));
    }
    return 0;
}
