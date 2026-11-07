/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../frame_codec.h"
static void pack(unsigned char *p,unsigned a,unsigned b,unsigned c,unsigned d)
{
 p[0]=(unsigned char)a;p[1]=(unsigned char)((a>>8)|(b<<2));
 p[2]=(unsigned char)((b>>6)|(c<<4));p[3]=(unsigned char)((c>>4)|(d<<6));p[4]=(unsigned char)(d>>2);
}
int main(void)
{
 unsigned char *raw=malloc(A04_HI556_FRAME_BYTES),*rgb=malloc(A04_HI556_RGB_BYTES+2);
 struct a04_hi556_content c;assert(raw&&rgb);
 memset(raw,0,A04_HI556_FRAME_BYTES);assert(a04_hi556_content_check(raw,A04_HI556_FRAME_BYTES,&c)&&!c.nonzero);
 memset(raw,255,A04_HI556_FRAME_BYTES);assert(a04_hi556_content_check(raw,A04_HI556_FRAME_BYTES,&c)&&!c.nonconstant);
 memset(raw,0xa5,A04_HI556_FRAME_BYTES);memset(rgb,0xa5,A04_HI556_RGB_BYTES+2);
 for(unsigned y=0;y<A04_HI556_HEIGHT;y++)for(unsigned x=0;x<A04_HI556_WIDTH;x+=4){
  if(!(y&1))pack(raw+y*A04_HI556_STRIDE+x/4*5,400,800,400,800);
  else pack(raw+y*A04_HI556_STRIDE+x/4*5,200,600,200,600);
 }
 assert(!a04_hi556_content_check(raw,A04_HI556_FRAME_BYTES,&c)&&c.minimum==200&&c.maximum==800);
 assert(!a04_hi556_rgb24(raw,A04_HI556_FRAME_BYTES,rgb+1,A04_HI556_RGB_BYTES));
 for(unsigned i=0;i<A04_HI556_RGB_BYTES;i+=3)assert(rgb[1+i]==200&&rgb[2+i]==125&&rgb[3+i]==50);
 assert(rgb[0]==0xa5&&rgb[A04_HI556_RGB_BYTES+1]==0xa5);
 for(unsigned y=0;y<A04_HI556_HEIGHT;y++)for(unsigned x=1620;x<1630;x++)assert(raw[y*A04_HI556_STRIDE+x]==0xa5);
 /* Position/top-bottom and asymmetric low bits, not an all-white fixture. */
 pack(raw,1,1022,9,1023);pack(raw+A04_HI556_STRIDE,14,3,7,11);
 assert(!a04_hi556_rgb24(raw,A04_HI556_FRAME_BYTES,rgb+1,A04_HI556_RGB_BYTES));
 {unsigned at=(A04_HI556_RGB_HEIGHT-1)*A04_HI556_RGB_STRIDE+1;
 assert(rgb[at]==255&&rgb[at+1]==0&&rgb[at+2]==3);}
 assert(a04_hi556_rgb24(raw,A04_HI556_FRAME_BYTES-1,rgb+1,A04_HI556_RGB_BYTES));
 assert(a04_hi556_rgb24(raw,A04_HI556_FRAME_BYTES,rgb+1,A04_HI556_RGB_BYTES-1));
 assert(a04_hi556_rgb24(raw,A04_HI556_FRAME_BYTES,raw,A04_HI556_RGB_BYTES));
 free(raw);free(rgb);return 0;
}
