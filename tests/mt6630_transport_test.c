/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "transport.h"
struct fake {unsigned calls;int fail;uint32_t last;};
static int req(void *ctx,uint32_t arg,unsigned write,uint8_t *data,size_t n)
{struct fake *f=ctx;(void)write;(void)data;(void)n;f->last=arg;f->calls++;return f->fail && f->calls==(unsigned)f->fail;}
int main(void)
{
 uint8_t data[2048]={0};struct fake f={0};struct a04_sdio_transport s={{&f,req},1,0};
 assert(!a04_sdio_transfer(&s,0,1,1,0x100,data,1025));assert(f.calls==3);assert(((f.last>>9)&0x1ffff)==0x500);
 f.calls=0;assert(a04_sdio_transfer(&s,0,1,1,0x1ffff,data,2)==-1&&!f.calls);
 assert(a04_sdio_transfer(&s,1,2,0,0x18,data,513)==-1&&!f.calls);
 assert(!a04_sdio_transfer(&s,1,2,0,0x18,data,1024)&&f.calls==1&&(f.last&(1u<<27)));
 f.calls=0;f.fail=2;assert(a04_sdio_transfer(&s,0,1,1,0,data,1024)==-2&&s.poisoned&&!s.ready&&f.calls==2);assert(a04_sdio_transfer(&s,0,1,1,0,data,1)==-1&&f.calls==2);
 puts("SDIO serialized transfer planning and poison: OK");return 0;
}
