/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "usbport_profile.h"
int main(void){
 unsigned i,v;
 assert(a04_usbport_native_route(0)&&a04_usbport_native_route(0xffff));
 for(i=1;i<=127;i++)assert(!a04_usbport_native_route(i));
 assert(!a04_usbport_native_route(0x80)&&!a04_usbport_native_route(0xfffe));
 for(i=1;i<=32;i++){
  int ok=!(i&(i-1));assert((a04_usbport_interval(2,3,i,&v)==0)==ok);
  if(ok)assert((1u<<(v-1))==i&&v>=1&&v<=6);
 }
 for(i=0;i<2;i++)for(unsigned n=1;n<=255;n++)assert(!a04_usbport_interval(i,3,n,&v)&&v==n);
 assert(a04_usbport_interval(2,3,0,&v)<0&&a04_usbport_interval(2,3,64,&v)<0);
 assert(a04_usbport_interval(3,3,1,&v)<0&&a04_usbport_interval(2,3,1,0)<0);
 assert(!a04_usbport_interval(2,2,0,&v)&&!v);
 return 0;
}
