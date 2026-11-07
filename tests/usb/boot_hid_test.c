/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "boot_hid.h"
/* Independent HID short-item walker: collection identities, IDs, bit offsets,
 * relative/absolute flags, signed ranges, and constant padding. */
static void descriptor(unsigned output)
{
 unsigned n,i=0,page=0,size=0,count=0,id=0,usage=0,depth=0,tlc=0;
 unsigned bits[3]={0},out[3]={0},buttons=0,axes=0,padding=0;int minimum=0,maximum=0;
 const unsigned char*d=A04BootHidDescriptor(output,&n);
 while(i<n){unsigned tag=d[i++],bytes=tag&3u,v=0,j;int sv;
  assert(tag!=0xfe);if(bytes==3)bytes=4;assert(i+bytes<=n);
  for(j=0;j<bytes;++j)v|=(unsigned)d[i+j]<<(8*j);
  sv=(bytes && bytes<4 && (d[i+bytes-1]&0x80))?(int)(v|(~0u<<(bytes*8))):(int)v;i+=bytes;
  switch(tag&0xfc){
  case 0x04:page=v;break;case 0x08:usage=v;break;
  case 0x14:minimum=sv;break;case 0x24:maximum=sv;break;
  case 0x74:size=v;break;case 0x94:count=v;break;
  case 0x84:id=v;assert(id==1||id==2);break;
  case 0xa0:if(!depth){assert(v==1 && page==1);assert(usage==(tlc?2u:6u));++tlc;}++depth;break;
  case 0xc0:assert(depth);--depth;break;
  case 0x80:
   assert(id && depth);
   if(id==2){
    if(v&1){padding+=size*count;}
    else if(page==9){assert(bits[id]==0 && size==1 && count==3 && v==2 && minimum==0 && maximum==1);++buttons;}
    else {assert(page==1 && usage==0x31 && bits[id]==8 && size==8 && count==2 && v==6 && minimum==-128 && maximum==127);++axes;}
   }
   bits[id]+=size*count;break;
  case 0x90:assert(id==1);out[id]+=size*count;break;
  default:break;
  }
 }
 assert(!depth && tlc==2 && bits[1]==64 && bits[2]==64 && out[1]==(output?8u:0u) && !out[2]);
 assert(buttons==1 && axes==1 && padding==45);
}
int main(void)
{
 unsigned i;unsigned char raw[8]={0xff,0x80,0x7f,0xaa,0xbb,0xcc,0xdd,0xee},win[10];KB_ENUM k={0};KB_CONTROL control;
 unsigned char led=0xff,packet[]={1,0x1f};descriptor(0);descriptor(1);
 memset(win,0x5a,sizeof(win));assert(!A04BootHidEncode(A04_BOOT_KEYBOARD,raw,win));assert(win[0]==1 && !memcmp(win+1,raw,8) && win[9]==0x5a);
 assert(!A04BootHidEncode(A04_BOOT_MOUSE,raw,win));assert(!memcmp(win,(unsigned char[]){2,7,0x80,0x7f,0,0,0,0,0},9)&&win[9]==0x5a);
 assert(!A04BootHidMouseSnapshot(win[1],win));assert(!memcmp(win,(unsigned char[]){2,7,0,0,0,0,0,0,0},9));
 for(i=0;i<256;++i){raw[1]=(unsigned char)i;assert(!A04BootHidEncode(2,raw,win)&&win[2]==i);}
 memset(win,0x5a,sizeof(win));assert(A04BootHidEncode(0,raw,win)<0&&win[0]==0x5a);assert(A04BootHidEncode(3,raw,win)<0);assert(A04BootHidEncode(2,0,win)<0);assert(A04BootHidEncode(2,raw,0)<0);
 k.stage=KB_READY;k.interface_number=7;k.max_packet0=8;k.protocol=A04_BOOT_KEYBOARD;
 assert(!A04BootHidKeyboardOutput(&k,packet,2,1,&control,&led)&&led==31);
 assert(!memcmp(control.setup,(unsigned char[]){0x21,9,0,2,7,0,1,0},8));
 k.protocol=A04_BOOT_MOUSE;assert(A04BootHidKeyboardOutput(&k,packet,2,1,&control,&led)<0);k.protocol=1;
 assert(A04BootHidKeyboardOutput(&k,packet,2,2,&control,&led)<0);packet[0]=0;assert(A04BootHidKeyboardOutput(&k,packet,2,1,&control,&led)<0);
 packet[0]=1;packet[1]=0x20;assert(A04BootHidKeyboardOutput(&k,packet,2,1,&control,&led)<0);assert(A04BootHidKeyboardOutput(&k,packet,1,1,&control,&led)<0);
 return 0;
}
