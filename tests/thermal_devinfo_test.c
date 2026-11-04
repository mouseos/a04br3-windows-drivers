/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "devinfo.h"
#include "thermal.h"
int main(void) {
 uint8_t buf[4+63*4]={0}; struct a04_devinfo_blob b={buf,sizeof buf}; uint32_t v=42;
 buf[0]=63;buf[4+60*4]=0x11;buf[4+60*4+1]=0x22;buf[4+60*4+2]=0x33;buf[4+60*4+3]=0x44;
 assert(!a04_devinfo_atag_read(&b,60,&v) && v==0x44332211u);
 assert(a04_devinfo_atag_read(&b,63,&v) && v==0x44332211u);
 b.size--;assert(a04_devinfo_atag_read(&b,60,&v));b.size=sizeof buf;
 buf[1]=2;assert(a04_devinfo_atag_read(&b,60,&v));buf[1]=0;
 buf[0]=60;assert(a04_devinfo_atag_read(&b,60,&v));
 puts("boot devinfo bounds, byte order and index checks: OK");return 0;
}
