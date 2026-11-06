/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-display/mt8167_ovl_saved.h"
int main(void)
{
    uint32_t a[A04_OVL_SNAPSHOT_WORDS]={1,9,0x05000320,0x20ff,0x05000320,0,0x00ff0c80,0xbe9c0000,0x21ff,0x05000320,0,0x00ff0c80,0xbeda8000},b[A04_OVL_SNAPSHOT_WORDS];
    memcpy(b,a,sizeof(a));assert(a04_ovl_saved_layout(a,b,0xbe9c0000,0xbeda8000,0xb8c1f000));
    assert(!a04_ovl_saved_layout(a,b,0xbe9c0000,0xbeda8000,0xbe8c0000));
    assert(!a04_ovl_saved_layout(a,b,0xbe9c0000,0xbeda8000,0xfffff000));
    assert(!a04_ovl_saved_layout(a,b,0xbe9c0000,0xbeda8004,0xb8c1f000));
    for(unsigned i=0;i<A04_OVL_SNAPSHOT_WORDS;i++){memcpy(b,a,sizeof(a));b[i]^=1u;assert(!a04_ovl_saved_layout(a,b,0xbe9c0000,0xbeda8000,0xb8c1f000));}
    memcpy(b,a,sizeof(a));a[1]=b[1]=1;assert(!a04_ovl_saved_layout(a,b,0xbe9c0000,0xbeda8000,0xb8c1f000));
    return 0;
}
