/* SPDX-License-Identifier: MIT */
#ifndef GE8300_TEXTURE_LAYOUT_H
#define GE8300_TEXTURE_LAYOUT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Rogue IMAGE_WORD0 dimensions are14bit minus-one; STRIDE_IMAGE_WORD1
 * has15bit pixels-minus-one. These are encoding bounds, not advertised caps.
 * Keep the existing real4MiB per-MDL budget; total VidMm residency remains
 * subject to the unchanged native aperture and allocation leases. */
#define GEO_TEXTURE_MAX_EDGE 16384u
#define GEO_TEXTURE_MAX_PITCH 131072u
#define GEO_TEXTURE_MAX_BYTES 4194304u
#define GEO_FORMAT_A8B8G8R8 32u /* D3DDDIFMT_A8B8G8R8: RGBA bytes on ARM LE. */
static inline int geo_texture_layout_valid(uint32_t w,uint32_t h,uint32_t pitch)
{
 return w&&h&&w<=GEO_TEXTURE_MAX_EDGE&&h<=GEO_TEXTURE_MAX_EDGE&&
  !(pitch&3u)&&pitch>=w*4u&&pitch<=GEO_TEXTURE_MAX_PITCH&&
  (uint64_t)pitch*h<=GEO_TEXTURE_MAX_BYTES;
}
/* Runtime has no requested internal rowpitch in CreateResource. Allocate
 * cache-line rows, return the actual pitch from Map, and upload input rows
 * using their independent SysMemPitch. Padding is not sampled as pixels. */
static inline uint32_t geo_texture_default_pitch(uint32_t width)
{return width&&width<=GEO_TEXTURE_MAX_EDGE?(width*4u+63u)&~63u:0u;}
/* CPU upload into the runtime-locked real resource, not GPU rendering. Input
 * SysMemPitch is independent and need not equal/alignment-match GPU stride. */
static inline int geo_texture_upload_rows(void *dst,uint32_t dst_pitch,
 const void *src,uint32_t src_pitch,uint32_t width,uint32_t height)
{
 uint32_t y,row;
 if(!dst||!src||!geo_texture_layout_valid(width,height,dst_pitch))return 1;
 row=width*4u;
 if(src_pitch<row||(uint64_t)(height-1u)*src_pitch+row>SIZE_MAX)return 1;
 for(y=0;y<height;y++)memcpy((uint8_t*)dst+(size_t)y*dst_pitch,(const uint8_t*)src+(size_t)y*src_pitch,row);
 return 0;
}
#endif
