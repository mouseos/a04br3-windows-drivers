/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "sdio_args.h"
int main(void) {
 uint32_t v=0,old=0xdeadbeef;unsigned fn;
 for(fn=0;fn<8;fn++) {assert(!a04_sdio_cmd52(1,fn,1,0x1ffff,0xa5,&v));assert(v==((1u<<31)|(fn<<28)|(1u<<27)|(0x1ffffu<<9)|0xa5));}
 assert(!a04_sdio_cmd53(0,1,0,1,0x100,512,&v)&&v==0x14020000u);
 assert(!a04_sdio_cmd53(1,2,1,0,0x44,511,&v)&&v==0xa80089ffu);
 v=old;assert(a04_sdio_cmd53(0,1,1,1,0,512,&v)&&v==old);
 assert(a04_sdio_cmd53(0,1,0,1,0,0,&v));assert(a04_sdio_cmd52(0,1,1,0,0,&v));assert(a04_sdio_cmd52(0,8,0,0,0,&v));
 assert(a04_sdio_r5_ok(0));assert(!a04_sdio_r5_ok(1u<<15));assert(!a04_sdio_r5_ok(1u<<8));puts("SDIO CMD52/CMD53 arguments and R5 failures: OK");return 0;
}
