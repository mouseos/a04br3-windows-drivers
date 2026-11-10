/* SPDX-License-Identifier: MIT */
#ifndef GEO_VIEWPORT_PROFILE_H
#define GEO_VIEWPORT_PROFILE_H
#include <stdint.h>
#include "render_layout.h"
/* Exact no-FP profile shared by typed admission and PPP emission.
 * Final bounds are checked against the captured target descriptor. */
static inline int geo_viewport_integral(uint32_t f,uint32_t max,uint32_t *out) {
 uint32_t exponent=(f>>23)&255u,significand=(f&0x7fffffu)|0x800000u;
 if(!(f&0x7fffffffu)){*out=0;return 1;}
 if((f>>31)||exponent<127||exponent>139)return 0;
 uint32_t shift=150-exponent;
 if(significand&((1u<<shift)-1u))return 0;
 uint32_t v=significand>>shift;if(v>max)return 0;*out=v;return 1;
}
static inline int geo_viewport_target_valid(const uint32_t f[6],uint32_t width,uint32_t height) {
 uint32_t x,y,w,h;
 return f&&geo_viewport_integral(f[0],width,&x)&&geo_viewport_integral(f[1],height,&y)&&
  geo_viewport_integral(f[2],width,&w)&&geo_viewport_integral(f[3],height,&h)&&w&&h&&
  x<=width-w&&y<=height-h&&!f[4]&&f[5]==0x3f800000u;
}
static inline int geo_viewport_profile_valid(const uint32_t f[6]) {
 return geo_viewport_target_valid(f,GEO_RENDER_MAX_EDGE,GEO_RENDER_MAX_EDGE);
}
#endif
