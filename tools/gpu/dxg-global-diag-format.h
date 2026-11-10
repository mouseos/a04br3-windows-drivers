/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef DXG_GLOBAL_DIAG_FORMAT_H
#define DXG_GLOBAL_DIAG_FORMAT_H
#include <stdint.h>
#define DXG_DIAG_CAPACITY (256u * 1024u)
struct dxg_diag_result {uint32_t records,tag23,negative_tag23,parsed_bytes,malformed,capacity_hit,first_negative_status,first_negative_offset;};
typedef void (*dxg_diag_emit)(void *,uint32_t,const unsigned char *);
static uint32_t dxg_diag_u32(const unsigned char *p) {
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
/* 保存RT8.1のDXGK_DIAG_HEADERを境界から走査する。resyncで偽statusを拾わない。 */
static struct dxg_diag_result dxg_diag_parse(const unsigned char *raw,uint32_t bytes,dxg_diag_emit emit,void *context) {
 struct dxg_diag_result r={0};uint32_t offset=0;
 if(bytes>DXG_DIAG_CAPACITY||(!raw&&bytes)){r.malformed=1;return r;}
 r.capacity_hit=bytes==DXG_DIAG_CAPACITY;
 while(offset<bytes){uint32_t size,type;
  if(bytes-offset<0x30u){r.malformed=1;break;}
  size=dxg_diag_u32(raw+offset+4);type=dxg_diag_u32(raw+offset);
  if(size<0x30u||size>bytes-offset){r.malformed=1;break;}
  if(type==6&&size<0x40u){r.malformed=1;break;}
  ++r.records;
  if(type==6&&size>=0x40u&&dxg_diag_u32(raw+offset+0x30)==0x23u){
   if(size!=0x40u){r.malformed=1;break;}
   ++r.tag23;if(dxg_diag_u32(raw+offset+0x3c)&0x80000000u){
    if(!r.negative_tag23){r.first_negative_status=dxg_diag_u32(raw+offset+0x3c);r.first_negative_offset=offset;}
    ++r.negative_tag23;
   }
   if(emit)emit(context,offset,raw+offset);
  }
  offset+=size;
 }
 r.parsed_bytes=offset;return r;
}
/* 証拠取得の完了判定。負tag23とAPI自体の失敗を混同しない。 */
static unsigned dxg_diag_snapshot_exit(uint32_t api_status,uint32_t returned,const struct dxg_diag_result *r) {
 if(api_status)return 3;
 if(returned>DXG_DIAG_CAPACITY)return 5;
 if(!r)return 4;
 if(r->malformed)return 5;
 if(r->capacity_hit)return 7;
 return r->tag23?0:4;
}
#endif
