#include <assert.h>
#include "pnp_resources.h"
int main(void)
{
 struct a04_usb_memory_pair p[2]={{0x11100000,0x21100000,0x10000},{0x11110000,0x31100000,0x10000}};
 uint64_t mac=1,phy=2;
 assert(!a04_usb_translate_memory(p,&mac,&phy) && mac==0x21100000 && phy==0x31100000);
 {struct a04_usb_memory_pair v=p[0];p[0]=p[1];p[1]=v;}
 assert(!a04_usb_translate_memory(p,&mac,&phy) && mac==0x21100000 && phy==0x31100000);
 p[1].translated=p[0].translated+0xffff;mac=1;phy=2;
 assert(a04_usb_translate_memory(p,&mac,&phy) && mac==1 && phy==2);
 p[1].translated=(uint64_t)INT64_MAX-3;assert(a04_usb_translate_memory(p,&mac,&phy));
 p[1].translated=0x21100000;p[1].raw=0x11120000;assert(a04_usb_translate_memory(p,&mac,&phy));
 p[1].raw=0x11100000;p[1].length=0x1000;assert(a04_usb_translate_memory(p,&mac,&phy));
 return 0;
}
