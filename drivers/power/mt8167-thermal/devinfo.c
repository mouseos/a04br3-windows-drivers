/* SPDX-License-Identifier: GPL-2.0-only
 * MediaTek Linux devinfo.c defines atag,devinfo as a native little-endian
 * u32 count followed by count u32 entries on this ARM32 device. */
#include "devinfo.h"
#include <string.h>
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
int a04_devinfo_atag_read(void *ctx, unsigned index, uint32_t *out)
{
    const struct a04_devinfo_blob *b=ctx; uint32_t count;
    if (!b || !b->bytes || !out || b->size<4) return -1;
    count=le32(b->bytes);
    if (count>300 || (size_t)count>(b->size-4)/4 || index>=count) return -1;
    *out=le32(b->bytes+4+(size_t)index*4); return 0;
}

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static int same(const uint8_t *s, size_t max, const char *name)
{ size_t i=0; while(i<max && name[i] && s[i]==(uint8_t)name[i]) i++; return i<max && !name[i] && !s[i]; }
int a04_devinfo_fdt_extract(const void *data, size_t bytes, uint32_t out[3])
{
 const uint8_t *p=data,*st,*strings; uint32_t total,so,ss,no,ns,token,len,nm; size_t pos=0,end; int depth=-1,chosen=-1,found=0,chosen_seen=0; uint32_t vals[3];
 struct a04_devinfo_blob blob;
 if(!p||!out||bytes<40 || be32(p)!=0xd00dfeedu) return -1;
 total=be32(p+4);so=be32(p+8);no=be32(p+12);ns=be32(p+32);ss=be32(p+36);
 if(total<40 || total>bytes || so>total || ss>total-so || no>total || ns>total-no || (so&3))return -1;
 st=p+so;strings=p+no;
 while(pos+4<=ss) {
  token=be32(st+pos);pos+=4;
  if(token==1) {
   end=pos;while(end<ss && st[end])end++;if(end==ss)return -1;
   depth++; if(depth==1 && (same(st+pos,end-pos+1,"chosen") || same(st+pos,end-pos+1,"chosen@0"))) { if(chosen_seen)return -1; chosen_seen=1;chosen=depth; }
   pos=(end+4)&~(size_t)3;if(pos>ss)return -1;
  } else if(token==2) { if(depth<0)return -1;if(depth==chosen)chosen=-2;depth--; }
  else if(token==3) {
   if(pos+8>ss)return -1;
   len=be32(st+pos);nm=be32(st+pos+4);pos+=8;
   if(nm>=ns || len>ss-pos || !memchr(strings+nm,0,ns-nm))return -1;
   if(depth==1 && chosen==1 && same(strings+nm,ns-nm,"atag,devinfo")) {
    if(found)return -1;
    blob.bytes=st+pos;blob.size=len;
    if(a04_devinfo_atag_read(&blob,60,&vals[0]) || a04_devinfo_atag_read(&blob,61,&vals[1]) || a04_devinfo_atag_read(&blob,62,&vals[2]))return -1;
    found=1;
   }
   if((((uint64_t)len+3u) & ~(uint64_t)3u)>(uint64_t)(ss-pos))return -1;
   end=(size_t)(((uint64_t)len+3u)&~(uint64_t)3u);pos+=end;
  } else if(token==4) continue;
  else if(token==9) { if(depth!=-1 || !found)return -1;out[0]=vals[0];out[1]=vals[1];out[2]=vals[2];return 0; }
  else return -1;
 }
 return -1;
}
