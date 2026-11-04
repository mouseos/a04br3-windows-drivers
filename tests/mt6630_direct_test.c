/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "direct.h"
struct fake { unsigned calls; uint32_t arg, response; int error, revoke; struct a04_wifi_owner *owner; };
static int request(void *ctx, uint32_t arg, uint32_t *response)
{
 struct fake *f = ctx; f->calls++; f->arg = arg; *response = f->response;
 if (f->revoke) a04_wifi_revoke(f->owner);
 return f->error;
}
int main(void)
{
 struct a04_wifi_owner owner = {0,1,1,1,1,1,0,0};
 struct fake f = {0,0,0x5a,0,0,&owner};
 struct a04_sdio_direct s = {&owner,&f,request,0,0}; uint8_t data = 0xaa;
 assert(!a04_wifi_admit(&owner));
 assert(!a04_sdio_direct52(&s,0,1,0,0x123,&data));
 assert(data==0x5a && f.calls==1 && f.arg==((1u<<28)|(0x123u<<9)));
 data=0xaa; assert(!a04_sdio_direct52(&s,1,2,0,2,&data)); assert(data==0xaa);
 assert(a04_sdio_direct52(&s,0,1,1,0,&data)==-1 && f.calls==2);
 f.response=1u<<9; assert(a04_sdio_direct52(&s,0,1,0,0,&data)==-2);
 assert(owner.poisoned && s.poisoned && data==0xaa);
 assert(a04_sdio_direct52(&s,0,1,0,0,&data)==-1 && f.calls==3);
 owner=(struct a04_wifi_owner){0,1,1,1,1,1,0,0}; s.poisoned=0; s.generation=0; f.response=0; f.revoke=1;
 assert(!a04_wifi_admit(&owner)); assert(a04_sdio_direct52(&s,0,1,0,0,&data)==-2 && owner.poisoned);
 puts("CMD52 lease, R5 and failure handling: OK"); return 0;
}
