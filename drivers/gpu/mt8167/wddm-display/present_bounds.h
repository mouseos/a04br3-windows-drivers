/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_PRESENT_BOUNDS_H
#define A04BR3_PRESENT_BOUNDS_H
#include <stdint.h>
#include <stddef.h>
/* Validate an upright source's full pitched span and the control allocation.
 * Negative/zero stride needs separate bottom-up support and is rejected. */
static inline int a04_present_bounds(uint32_t width,uint32_t height,uint32_t bytes,
    int32_t pitch,uint32_t moves,size_t move_size,uint32_t rects,size_t rect_size,
    size_t context_size,size_t *allocation,uint32_t *mapped_span)
{
    uint64_t row,span; size_t n;
    if (!width || !height || !bytes || pitch<=0 || !allocation || !mapped_span) return -1;
    row=(uint64_t)width*bytes;
    if (row>(uint32_t)pitch) return -1;
    span=(uint64_t)(height-1u)*(uint32_t)pitch+row;
    if (span>UINT32_MAX) return -1;
    n=context_size;
    if (moves && move_size>(SIZE_MAX-n)/moves) return -1;
    n+=(size_t)moves*move_size;
    if (rects && rect_size>(SIZE_MAX-n)/rects) return -1;
    n+=(size_t)rects*rect_size;
    *allocation=n; *mapped_span=(uint32_t)span; return 0;
}
static inline int a04_present_rect(int32_t left,int32_t top,int32_t right,int32_t bottom,
                                   uint32_t width,uint32_t height)
{
    if (left<0 || top<0 || right<=left || bottom<=top ||
        (uint32_t)right>width || (uint32_t)bottom>height) return -1;
    return 0;
}
static inline int a04_present_move(int32_t left,int32_t top,int32_t right,int32_t bottom,
    int32_t source_x,int32_t source_y,uint32_t width,uint32_t height)
{
    if (a04_present_rect(left,top,right,bottom,width,height) || source_x<0 || source_y<0 ||
        (uint64_t)(uint32_t)source_x+(uint32_t)(right-left)>width ||
        (uint64_t)(uint32_t)source_y+(uint32_t)(bottom-top)>height) return -1;
    return 0;
}
/* Compute the physical destination extent and a centered byte offset. Turns
 * are counter-clockwise D3DKMDT quarter turns; all geometry is checked before pointer use. */
static inline int a04_present_layout(uint32_t width,uint32_t height,uint32_t fb_width,
    uint32_t fb_height,uint32_t pitch,unsigned turns,int centered,
    uint32_t *physical_width,uint32_t *physical_height,uint32_t *offset)
{
    uint32_t w,h,x,y; uint64_t total,start,end;
    if (!width || !height || width>INT32_MAX || height>INT32_MAX || !fb_width || !fb_height || pitch>INT32_MAX || turns>3 || !physical_width ||
        !physical_height || !offset || (uint64_t)fb_width*4u>pitch) return -1;
    w=(turns&1u) ? height : width; h=(turns&1u) ? width : height;
    if (w>fb_width || h>fb_height || (!centered && (w!=fb_width || h!=fb_height))) return -1;
    x=centered ? (fb_width-w)/2u : 0; y=centered ? (fb_height-h)/2u : 0;
    total=(uint64_t)pitch*fb_height;
    start=(uint64_t)y*pitch+(uint64_t)x*4u;
    end=start+(uint64_t)(h-1u)*pitch+(uint64_t)w*4u;
    if (total>UINT32_MAX || end>total || start>UINT32_MAX) return -1;
    *physical_width=w; *physical_height=h; *offset=(uint32_t)start; return 0;
}
/* WDK8.1 rotation values are IDENTITY1/90=2/180=3/270=4.
 * Uninitialized/unspecified paths choose identity; UNPINNED is not executable. */
static inline int a04_present_turns(unsigned rotation,unsigned *turns)
{
    if(!turns)return -1;
    if(rotation==0u || rotation==255u){*turns=0;return 0;}
    if(rotation<1u || rotation>4u)return -1;
    *turns=rotation-1u;return 0;
}
/* Rotate==0 means the supplied pixels must not be rotated again (DDI).
 * In an odd-rotated path the already-oriented buffer is target-shaped. */
static inline int a04_present_source_shape(uint32_t width,uint32_t height,
    unsigned path_turns,unsigned copy_turns,uint32_t *source_width,uint32_t *source_height)
{
    if(!width || !height || path_turns>3 || copy_turns>3 || !source_width || !source_height ||
       (copy_turns!=0 && copy_turns!=path_turns))return -1;
    *source_width=(copy_turns==0 && (path_turns&1u)) ? height:width;
    *source_height=(copy_turns==0 && (path_turns&1u)) ? width:height;
    return 0;
}
/* GPU staging only accepts full-width, tightly packed, unrotated rows. */
static inline int a04_present_gpu_span(uint32_t width,uint32_t height,uint32_t dst_width,
    uint32_t dst_height,uint32_t src_pitch,uint32_t dst_pitch,int32_t left,int32_t top,
    int32_t right,int32_t bottom,uint32_t *offset,uint32_t *bytes)
{
    uint64_t start,length;
    if (!offset || !bytes || !width || !height || !src_pitch || src_pitch!=dst_pitch ||
        (uint64_t)width*4u!=src_pitch || dst_width<width || dst_height<height ||
        left!=0 || top<0 || right<=0 || (uint32_t)right!=width || bottom<=top ||
        (uint32_t)bottom>height) return -1;
    start=(uint64_t)(uint32_t)top*src_pitch;
    length=(uint64_t)(uint32_t)(bottom-top)*src_pitch;
    if (length<16384u || start+length>UINT32_MAX) return -1;
    *offset=(uint32_t)start; *bytes=(uint32_t)length; return 0;
}
#endif
