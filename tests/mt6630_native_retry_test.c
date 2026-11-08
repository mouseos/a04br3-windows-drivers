/* Production retry owner: no radio, fabricated association or key success. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../drivers/network/mt6630/native_retry.h"
static unsigned frame(uint8_t*p,unsigned qos,unsigned tid,unsigned retry,unsigned seq){unsigned h=qos?26:24;memset(p,0,64);p[0]=qos?0x88:8;p[1]=2|(retry?8:0);p[4]=2;p[22]=(seq<<4)&255;p[23]=seq>>4;if(qos)p[24]=tid;memcpy(p+h,"\xaa\xaa\x03\0\0\0\x08\0",8);return h+8;}
int main(void){struct a04_native_retry s={0};struct a04_native_retry_ticket t;uint8_t p[64],saved[64];unsigned n;
 n=frame(p,0,0,1,0);memcpy(saved,p,64);assert(!a04_native_retry_check(&s,7,p,n,&t));assert(!s.valid);a04_native_retry_commit(&s,&t);assert(a04_native_retry_check(&s,7,p,n,&t)==1&&!memcmp(saved,p,64));
 /* A failed filter/queue admission has no commit: the actual retry survives. */
 n=frame(p,0,0,0,3);assert(!a04_native_retry_check(&s,7,p,n,&t));p[1]|=8;assert(!a04_native_retry_check(&s,7,p,n,&t));a04_native_retry_commit(&s,&t);assert(a04_native_retry_check(&s,7,p,n,&t)==1);
 /* Non-retry same SC is a new transmission; 4095->0 wrap uses equality. */
 p[1]&=~8u;assert(!a04_native_retry_check(&s,7,p,n,&t));n=frame(p,0,0,1,4095);assert(!a04_native_retry_check(&s,7,p,n,&t));a04_native_retry_commit(&s,&t);n=frame(p,0,0,1,0);assert(!a04_native_retry_check(&s,7,p,n,&t));a04_native_retry_commit(&s,&t);
 assert(!a04_native_retry_check(&s,8,p,n,&t));a04_native_retry_commit(&s,&t);assert(s.valid==(1u<<4));
 for(unsigned tid=0;tid<8;tid++){n=frame(p,1,tid,1,tid+1);assert(!a04_native_retry_check(&s,9,p,n,&t));a04_native_retry_commit(&s,&t);assert(a04_native_retry_check(&s,9,p,n,&t)==1);}
 /* ERP/non-HT stock maps TID0/3 into the same BE sequence cache. */
 n=frame(p,1,0,0,50);assert(!a04_native_retry_check(&s,10,p,n,&t));a04_native_retry_commit(&s,&t);n=frame(p,1,3,1,50);assert(a04_native_retry_check(&s,10,p,n,&t)==1);
 n=frame(p,0,0,1,50);assert(!a04_native_retry_check(&s,10,p,n,&t));p[4]|=1;assert(a04_native_retry_check(&s,10,p,n,&t)<0);p[1]&=~8u;assert(!a04_native_retry_check(&s,10,p,n,&t));a04_native_retry_commit(&s,&t);
 p[1]|=4;assert(a04_native_retry_check(&s,10,p,n,&t)<0);p[1]=2;p[22]|=1;assert(a04_native_retry_check(&s,10,p,n,&t)<0);
 puts("FromDS first retry, duplicate suppression, queue-commit/epoch/AC separation and wrap PASS; radio untested");}
