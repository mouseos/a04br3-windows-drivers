/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "transport.h"
#include "sdio_args.h"
/* Fixed FIFO transfers use 512-byte blocks above 512. Incrementing transfers
 * use bounded byte mode and reject the 17-bit address wrap before any I/O. */
int a04_sdio_transfer(struct a04_sdio_transport *s,unsigned write,unsigned fn,unsigned incr,unsigned addr,uint8_t *data,size_t length)
{
 size_t done=0;
 if(!s||!s->ready||s->poisoned||!s->bus.request||!data||!length||write>1||fn>7||incr>1||addr>0x1ffffu)return -1;
 if(incr && length>(size_t)(0x20000u-addr))return -1;
 if(!incr && length>512 && length%512)return -1;
 while(done<length) {
  size_t remaining=length-done,chunk;unsigned block=0,count;uint32_t arg;
  if(!incr && remaining>=512) {block=1;count=(unsigned)(remaining/512>128?128:remaining/512);chunk=(size_t)count*512;}
  else {chunk=remaining>512?512:remaining;count=(unsigned)chunk;}
  if(a04_sdio_cmd53(write,fn,block,incr,addr+(incr?(unsigned)done:0),count,&arg))return -1;
  if(s->bus.request(s->bus.context,arg,write,data+done,chunk)) {s->poisoned=1;s->ready=0;return -2;}
  done+=chunk;
 }
 return 0;
}
