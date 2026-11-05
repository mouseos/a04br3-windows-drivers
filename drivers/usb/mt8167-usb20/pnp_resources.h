/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USB_PNP_RESOURCES_H
#define A04_USB_PNP_RESOURCES_H
#include <stdint.h>
/* Memory addresses in the raw and translated descriptors are paired by PnP.
 * The caller validates descriptor types, interrupt flags and exclusivity. */
struct a04_usb_memory_pair { uint64_t raw,translated; uint32_t length; };
static int a04_usb_translate_memory(const struct a04_usb_memory_pair *p,
                                    uint64_t *mac,uint64_t *phy)
{
    uint64_t m=0,y=0,a,b;
    if(!p || !mac || !phy)return -1;
    if(p[0].length!=0x10000u || p[1].length!=0x10000u)return -1;
    a=p[0].translated;b=p[1].translated;
    if(!a || !b || a>(uint64_t)INT64_MAX-0xffffu || b>(uint64_t)INT64_MAX-0xffffu ||
       (a<=b ? b-a : a-b)<0x10000u)return -1;
    if(p[0].raw==0x11100000u && p[1].raw==0x11110000u){m=a;y=b;}
    else if(p[1].raw==0x11100000u && p[0].raw==0x11110000u){m=b;y=a;}
    else return -1;
    *mac=m;*phy=y;return 0;
}
#endif
