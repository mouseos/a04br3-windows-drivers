/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MTK GPT NVRAM read-only runtime WIFI lookup. Geometry and checksums follow
 * libnvram NVM_GetDeviceInfo/RestoreFromBinRegion_OneFile/CheckData_OneFile/
 * ComputeCheckNo. Records are original on-device bytes, never defaults.
 * Adapted from the local untracked UniXNU-a04br3 file with SHA-256
 * 0cb8f284d7b52f8f9348418e6f7cf6af3517c99b25f3fad81f7a0b76f61d574a.
 * Caller serializes controller access. read() is bounded to this partition.
 */
#ifndef A04BR3_WLAN_NVRAM_H
#define A04BR3_WLAN_NVRAM_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum { A04_NVRAM_INVALID=1, A04_NVRAM_NOT_FOUND=2 };
#define A04_NVRAM_BLOCK 0x20000u
#define A04_NVRAM_MAX_BLOCKS 512u
struct a04_nvram_reader {void *context;uint32_t bytes;int (*read)(void *,uint32_t,void *,unsigned);};
struct a04_nvram_work {uint8_t bitmap[A04_NVRAM_MAX_BLOCKS];uint8_t scratch[A04_NVRAM_MAX_BLOCKS*3+4];uint8_t record[514];};
static inline uint32_t a04_nvram_le32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline int a04_nvram_read(const struct a04_nvram_reader *r,uint32_t off,void *out,unsigned n)
{
 if(!r || !r->read || !out || off>r->bytes || n>r->bytes-off)return A04_NVRAM_INVALID;
 return r->read(r->context,off,out,n);
}
/* Logical AllFile blocks skip the same bitmap-marked bad blocks as libnvram. */
static inline int a04_nvram_file_read(const struct a04_nvram_reader *r,const struct a04_nvram_work *w,
 unsigned blocks,unsigned start,uint32_t off,void *out,unsigned n)
{
 uint8_t *p=(uint8_t *)out;unsigned index=off/A04_NVRAM_BLOCK,inside=off%A04_NVRAM_BLOCK,b=start;
 for(;b<blocks;b++)if(!w->bitmap[b]){if(!index)break;index--;}
 while(n){
  if(b>=blocks)return A04_NVRAM_INVALID;
  unsigned part=A04_NVRAM_BLOCK-inside;if(part>n)part=n;
  int e=a04_nvram_read(r,b*A04_NVRAM_BLOCK+inside,p,part);if(e)return e;
  n-=part;p+=part;inside=0;
  do{b++;}while(b<blocks && w->bitmap[b]);
 }
 return 0;
}
static inline int a04_nvram_wifi(const struct a04_nvram_reader *r,struct a04_nvram_work *w,uint8_t out[512])
{
 if(!r || !w || !out || r->bytes%A04_NVRAM_BLOCK)return A04_NVRAM_INVALID;
 unsigned total=r->bytes/A04_NVRAM_BLOCK;
 if(total<3 || total>A04_NVRAM_MAX_BLOCKS)return A04_NVRAM_INVALID;
 /* Match libnvram's descending search and triple bitmap + 0xaabbccdd flag.
  * The bitmap length is recovered from its flag position, not a baked-in GPT
  * partition length or a backup's offsets. No speculative file bytes accepted. */
 unsigned blocks=0,bitmap_block=0;
 for(unsigned at=total;at>0 && !blocks;){
  at--;int e=a04_nvram_read(r,at*A04_NVRAM_BLOCK,w->scratch,total*3+4);if(e)return e;
  for(unsigned n=at+1;n<=total;n++){
   if(a04_nvram_le32(w->scratch+3*n)!=0xaabbccddu)continue;
   unsigned valid=1;
   for(unsigned i=0;i<n;i++)if(w->scratch[i]!=w->scratch[i+n] || w->scratch[i]!=w->scratch[i+2*n]){valid=0;break;}
   if(valid){blocks=n;bitmap_block=at;memcpy(w->bitmap,w->scratch,n);break;}
  }
 }
 if(!blocks)return A04_NVRAM_NOT_FOUND;
 unsigned map=0;while(map<bitmap_block && w->bitmap[map])map++;
 if(map>=bitmap_block)return A04_NVRAM_INVALID;
 int e=a04_nvram_read(r,map*A04_NVRAM_BLOCK,w->scratch,64);if(e)return e;
 uint32_t mapsize=a04_nvram_le32(w->scratch),datasize=a04_nvram_le32(w->scratch+4);
 uint32_t expected=a04_nvram_le32(w->scratch+12);
 if(a04_nvram_le32(w->scratch+8)!=0xffffffffu || mapsize<52 || (mapsize-52)%140 ||
    mapsize>A04_NVRAM_BLOCK-12 || !datasize || datasize>r->bytes)return A04_NVRAM_INVALID;
 unsigned data=map+1;while(data<bitmap_block && w->bitmap[data])data++;
 if(data>=bitmap_block)return A04_NVRAM_INVALID;
 /* AllMap uses 12-byte container header then 52-byte map header. */
 uint32_t wifi_offset=0;unsigned matches=0;
 static const char name[]="/data/nvram/APCFG/APRDEB/WIFI";
 for(uint32_t at=52;at<mapsize;at+=140){
  e=a04_nvram_read(r,map*A04_NVRAM_BLOCK+12+at,w->scratch,140);if(e)return e;
  uint32_t offset=a04_nvram_le32(w->scratch+4),size=a04_nvram_le32(w->scratch+8);
  if(!size || offset>datasize || size>datasize-offset)return A04_NVRAM_INVALID;
  if(!memcmp(w->scratch+12,name,sizeof(name))){
   if(a04_nvram_le32(w->scratch)!=7 || size!=514 || matches++)return A04_NVRAM_INVALID;
   wifi_offset=offset;
  }
 }
 if(matches!=1)return A04_NVRAM_NOT_FOUND;
 uint32_t checksum=0;
 for(uint32_t off=0;off<datasize;){
  unsigned n=datasize-off;if(n>512)n=512;
  e=a04_nvram_file_read(r,w,bitmap_block,data,off,w->scratch,n);if(e)return e;
  unsigned whole=n&~3u;
  for(unsigned i=0;i<whole;i+=4){uint32_t v=a04_nvram_le32(w->scratch+i);checksum=((off+i)&4)?checksum+v:checksum^v;}
  uint32_t tail=0;for(unsigned i=whole;i<n;i++)tail|=(uint32_t)w->scratch[i]<<(8*(i-whole));
  checksum+=tail;off+=n;
 }
 if((checksum^datasize)!=expected)return A04_NVRAM_INVALID;
 e=a04_nvram_file_read(r,w,bitmap_block,data,wifi_offset,w->record,514);if(e)return e;
 unsigned check=0;for(unsigned i=0;i<512;i++)check=((i&1)?check^w->record[i]:check+w->record[i])&255;
 if(w->record[512]!=0xaa || w->record[513]!=check)return A04_NVRAM_INVALID;
 unsigned any=0;for(unsigned i=4;i<10;i++)any|=w->record[i];
 if(!any || (w->record[4]&1) || w->record[196]!=1)return A04_NVRAM_INVALID;
 memcpy(out,w->record,512);return 0;
}
/* No device reader is provided. Output is original on-device bytes only. */
int a04_mt6630_calibration_read(const struct a04_nvram_reader *,
 struct a04_nvram_work *,uint8_t calibration[512]);
#endif
