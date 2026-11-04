/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Adapted from UniXNU-a04br3 package/wifi/sdio/A04BR3SDIOPIO.h,
 * Local untracked source SHA-256 c0b519f9a7595621eb95aa02f2c16d65c9893d144127dbdde449e2d8d39b50d7.
 * That file was not part of the checkout HEAD.
 * https://github.com/mouseos/UniXNU-a04br3
 */
#include "msdc2_pio.h"
#include "sdio_args.h"
static int fail(struct a04_msdc2_pio *s)
{ s->poisoned=1; a04_wifi_fault(s->owner); return -2; }
int a04_msdc2_cmd53(void *context,uint32_t arg,unsigned write,uint8_t *p,size_t length)
{
 struct a04_msdc2_pio *s=context;
 unsigned block,blocks,blksz,n,done=0,budget; int ready=0,complete=0;
 uint32_t st,cfg;
 if(!s || !s->owner || !s->read || !s->write || !s->delay || s->poisoned ||
    s->owner->poisoned || !s->owner->active || s->generation!=s->owner->generation ||
    !p || !length || length>65536 || write>1 || write!=(arg>>31))return -1;
 block=(arg>>27)&1u;blocks=block?(arg&511u):1u;blksz=block?512u:(unsigned)length;
 if(!blocks || (block ? length!=(size_t)blocks*512u :
    (length>512 || (arg&511u)!=(length&511u))))return -1;
 for(n=0;n<10000;n++){if(!(s->read(s->context,0x3c,4)&3u))break;s->delay(s->context,10);}
 if(n==10000)return fail(s);
 s->write(s->context,0x14,1u<<31,4);
 for(n=0;n<10000;n++){if(!(s->read(s->context,0x14,4)&(1u<<31)))break;s->delay(s->context,10);}
 if(n==10000)return fail(s);
 s->write(s->context,0x0c,0xd700,4);s->write(s->context,0x50,blocks,4);
 cfg=s->read(s->context,0x208,4);
 s->write(s->context,0x208,(cfg&~(0x3ffu<<11))|((!write&&blksz>256?blksz+17:273u)<<11),4);
 s->write(s->context,0x38,arg,4);
 s->write(s->context,0x34,53u|(1u<<7)|((blocks>1?2u:1u)<<11)|(write<<13)|(blksz<<16),4);
 budget=10000u+(unsigned)length*4u;
 for(n=0;n<budget;n++){
  unsigned available,count;
  st=s->read(s->context,0x0c,4);
  if(st&0xc600u)return fail(s);
  if(st&0x100u){s->write(s->context,0x0c,0x100,4);if(!a04_sdio_r5_ok(s->read(s->context,0x40,4)))return fail(s);ready=1;}
  if(st&0x1000u){s->write(s->context,0x0c,0x1000,4);complete=1;}
  if(complete && write && done<length)return fail(s);
  if(ready && done<length){
   cfg=s->read(s->context,0x14,4);available=write?(((cfg>>16)&255u)?0u:128u):(cfg&255u);
   count=(unsigned)length-done;if(count>available)count=available;
   while(count){unsigned width=count>=4?4u:1u,k;uint32_t v=0;
    if(write){for(k=0;k<width;k++)v|=(uint32_t)p[done+k]<<(k*8);s->write(s->context,0x18,v,width);}
    else{v=s->read(s->context,0x1c,width);for(k=0;k<width;k++)p[done+k]=(uint8_t)(v>>(k*8));}
    done+=width;count-=width;
   }
  }
  if(ready && complete && done==length)return 0;
  s->delay(s->context,10);
 }
 return fail(s);
}

/* CMD52 R5: the caller validates response status. Only owned command-status
 * bits are cleared; the owner must have masked and quiesced the ISR. */
int a04_msdc2_cmd52(void *context,uint32_t arg,uint32_t *response)
{
 struct a04_msdc2_pio *s=context;unsigned n;uint32_t st;
 if(!s || !s->owner || !s->read || !s->write || !s->delay || !response ||
    s->poisoned || s->owner->poisoned || !s->owner->active ||
    s->generation!=s->owner->generation)return -1;
 for(n=0;n<10000;n++){if(!(s->read(s->context,0x3c,4)&1u))break;s->delay(s->context,10);}
 if(n==10000)return fail(s);
 st=s->read(s->context,0x0c,4)&0x700u;if(st)s->write(s->context,0x0c,st,4);
 s->write(s->context,0x38,arg,4);s->write(s->context,0x34,52u|(1u<<7),4);
 for(n=0;n<10000;n++){
  st=s->read(s->context,0x0c,4)&0x700u;
  if(st){s->write(s->context,0x0c,st,4);if((st&0x600u)||!(st&0x100u))return fail(s);
   *response=s->read(s->context,0x40,4);return 0;}
  s->delay(s->context,10);
 }
 return fail(s);
}
