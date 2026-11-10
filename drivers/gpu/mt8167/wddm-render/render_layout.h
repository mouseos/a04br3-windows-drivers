/* SPDX-License-Identifier: MIT */
#ifndef GEO_RENDER_LAYOUT_H
#define GEO_RENDER_LAYOUT_H
#include <stdint.h>
/* Existing owner private output capacity, not a new advertised D3D limit.
 * PBE surface: row stride in 32-pixel units; terminate stream: <=4096 edges.
 * Cold tile/TPC allocation additionally checks A04_NATIVE_DATA_LIMIT. */
#define GEO_RENDER_MAX_EDGE 4096u
#define GEO_RENDER_MAX_BYTES 4096000u
static inline uint32_t geo_render_pitch(uint32_t width) {
 return width && width<=GEO_RENDER_MAX_EDGE ? (width*4u+127u)&~127u : 0;
}
static inline int geo_render_layout_valid(uint32_t w,uint32_t h,uint32_t pitch) {
 return w && h && w<=GEO_RENDER_MAX_EDGE && h<=GEO_RENDER_MAX_EDGE &&
  pitch>=w*4u && pitch<=GEO_RENDER_MAX_EDGE*4u && !(pitch&127u) &&
  (uint64_t)pitch*h<=GEO_RENDER_MAX_BYTES;
}
#endif
