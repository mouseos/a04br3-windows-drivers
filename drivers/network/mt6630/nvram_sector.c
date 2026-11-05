/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "nvram_sector.h"
static int sector_read(void *context,uint32_t offset,void *output,unsigned bytes)
{
 struct a04_nvram_sector_reader *s=(struct a04_nvram_sector_reader *)context;
 uint8_t *p=(uint8_t *)output;
 uint64_t capacity=s->sector_count*512u;
 if(!p || offset>capacity || bytes>capacity-offset)return A04_NVRAM_INVALID;
 while(bytes){
  unsigned inside=offset%512u, n=512u-inside;
  if(n>bytes)n=bytes;
  int e=s->read_sector(s->context,s->first_lba+offset/512u,s->scratch);
  if(e)return e;
  memcpy(p,s->scratch+inside,n);
  p+=n;offset+=n;bytes-=n;
 }
 return 0;
}
int a04_nvram_sector_init(struct a04_nvram_sector_reader *s,struct a04_nvram_reader *r,
 void *context,int (*read_sector)(void *,uint64_t,uint8_t [512]),
 uint64_t first,uint64_t count,uint64_t disk_sectors)
{
 if(!s || !r || !read_sector || !count || first>=disk_sectors ||
    count>disk_sectors-first || count>UINT32_MAX/512u ||
    (count*512u)%A04_NVRAM_BLOCK)return A04_NVRAM_INVALID;
 s->context=context;s->read_sector=read_sector;s->first_lba=first;s->sector_count=count;
 r->context=s;r->bytes=(uint32_t)(count*512u);r->read=sector_read;
 return 0;
}
