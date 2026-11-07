/* SPDX-License-Identifier: GPL-2.0-only */
#include "frame_codec.h"
static unsigned pixel(const uint8_t *row,unsigned x)
{
 const uint8_t *p=row+(x>>2)*5u;
 switch(x&3u){
 case 0:return p[0]|((p[1]&3u)<<8);
 case 1:return (p[1]>>2)|((p[2]&15u)<<6);
 case 2:return (p[2]>>4)|((p[3]&63u)<<4);
 default:return (p[3]>>6)|((unsigned)p[4]<<2);
 }
}
int a04_hi556_content_check(const uint8_t *raw,size_t bytes,struct a04_hi556_content *c)
{
 if(!raw||!c||bytes!=A04_HI556_FRAME_BYTES)return -1;
 c->minimum=1023;c->maximum=c->nonzero=c->nonconstant=0;
 for(unsigned y=0;y<A04_HI556_HEIGHT;y++)for(unsigned x=0;x<A04_HI556_WIDTH;x++){
  unsigned v=pixel(raw+y*A04_HI556_STRIDE,x);
  if(v<c->minimum)c->minimum=v;
  if(v>c->maximum)c->maximum=v;
  c->nonzero+=v!=0;
 }
 c->nonconstant=c->minimum!=c->maximum;
 return c->nonzero&&c->nonconstant?0:-1;
}
int a04_hi556_rgb24(const uint8_t *raw,size_t bytes,uint8_t *rgb,size_t capacity)
{
 struct a04_hi556_content c;uintptr_t in=(uintptr_t)raw,out=(uintptr_t)rgb;
 if(!raw||!rgb||bytes!=A04_HI556_FRAME_BYTES||capacity<A04_HI556_RGB_BYTES)return -1;
 if(out>=in?out-in<bytes:in-out<A04_HI556_RGB_BYTES)return -1;
 if(a04_hi556_content_check(raw,bytes,&c))return -1;
 for(unsigned y=0;y<A04_HI556_RGB_HEIGHT;y++){
  const uint8_t *a=raw+(2*y)*A04_HI556_STRIDE,*b=a+A04_HI556_STRIDE;
  /* Positive-height RGB24 DIB: B,G,R and bottom-up, DWORD stride1944. */
  uint8_t *p=rgb+(A04_HI556_RGB_HEIGHT-1-y)*A04_HI556_RGB_STRIDE;
  for(unsigned x=0;x<A04_HI556_RGB_WIDTH;x++){
   p[3*x]=(uint8_t)(pixel(a,2*x+1)>>2);
   p[3*x+1]=(uint8_t)(((pixel(a,2*x)+pixel(b,2*x+1)+1)>>1)>>2);
   p[3*x+2]=(uint8_t)(pixel(b,2*x)>>2);
  }
 }
 return 0;
}
