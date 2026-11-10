/* SPDX-License-Identifier: MIT */
#ifndef GEO_UPDATE_SUBRESOURCE_H
#define GEO_UPDATE_SUBRESOURCE_H
#include <stdint.h>
#include <stddef.h>
#include "../texture_layout.h"
struct geo_update_region {uint32_t left,top,right,bottom,front,back;};
struct geo_update_plan {uint32_t left,top,row,rows,src_pitch;size_t src_bytes;};
/* Returns 0 for data, 1 for a documented empty-box no-op, -1 for invalid.
 * Source starts at the first texel to copy, never at destination box offset.
 * The API supplies source storage; its readable length is not a DDI argument.
 * Validate all representable source offsets, not an invented buffer length. */
static int geo_update_plan_2d(uint32_t width,uint32_t height,const struct geo_update_region *box,
 uint32_t src_pitch,struct geo_update_plan *out)
{
 struct geo_update_region b={0,0,width,height,0,1};uint64_t bytes;
 if(!out||!geo_texture_layout_valid(width,height,geo_texture_default_pitch(width)))return -1;
 if(box)b=*box;
 /* An empty box has no memory/synchronization side effects. */
 if(b.left>=b.right||b.top>=b.bottom||b.front>=b.back)return 1;
 if(b.front||b.back!=1||b.right>width||b.bottom>height)return -1;
 out->left=b.left;out->top=b.top;out->row=(b.right-b.left)*4u;out->rows=b.bottom-b.top;out->src_pitch=src_pitch;
 if(out->rows>1&&src_pitch<out->row)return -1;
 bytes=(uint64_t)(out->rows-1)*src_pitch+out->row;
 if(bytes>SIZE_MAX)return -1;
 out->src_bytes=(size_t)bytes;return 0;
}
static int geo_update_storage_2d(const struct geo_update_plan *p,uint32_t width,uint32_t height,
 uint32_t pitch,uint32_t capacity,const void *dst,const void *src)
{
 uint64_t end;uintptr_t d=(uintptr_t)dst,s=(uintptr_t)src;
 if(!p||!dst||!src||!geo_texture_layout_valid(width,height,pitch)||
    (uint64_t)pitch*height>capacity||!p->rows||!p->row||p->row>pitch||p->top>=height||p->rows>height-p->top||
    p->left>width||p->row/4u>width-p->left)return -1;
 end=(uint64_t)(p->top+p->rows-1u)*pitch+(uint64_t)p->left*4u+p->row;
 if(end>capacity||capacity>UINTPTR_MAX-d||p->src_bytes>UINTPTR_MAX-s)return -1;
 /* DEFAULT destinations cannot be mapped by the application. Reject aliasing
  * instead of invoking overlapping memcpy or claiming DISCARD renaming. */
 if(d<s+p->src_bytes&&s<d+capacity)return -1;
 return 0;
}
static void geo_update_copy_2d(const struct geo_update_plan *p,void *dst,uint32_t pitch,const void *src)
{
 uint32_t y;
 for(y=0;y<p->rows;y++)memcpy((uint8_t*)dst+(size_t)(p->top+y)*pitch+(size_t)p->left*4u,
  (const uint8_t*)src+(size_t)y*p->src_pitch,p->row);
}
#endif
