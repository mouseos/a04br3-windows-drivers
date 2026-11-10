/* SPDX-License-Identifier: MIT */
#ifndef GEO_VIEWPORT_PROFILE_H
#define GEO_VIEWPORT_PROFILE_H
#include <stdint.h>
/* Exact no-FP profile shared by typed admission and PPP emission. Bounds are
 * the existing 800x600 draw profile, not the larger physical RT backing. */
static inline int geo_viewport_integral(uint32_t f,uint32_t max,uint32_t *out) {
 uint32_t exponent=(f>>23)&255u,significand=(f&0x7fffffu)|0x800000u;
 if(!(f&0x7fffffffu)){*out=0;return 1;}
 if((f>>31)||exponent<127||exponent>136)return 0;
 uint32_t shift=150-exponent;
 if(significand&((1u<<shift)-1u))return 0;
 uint32_t v=significand>>shift;if(v>max)return 0;*out=v;return 1;
}
static inline int geo_viewport_profile_valid(const uint32_t f[6]) {
 uint32_t x,y,w,h;
 return f&&geo_viewport_integral(f[0],799,&x)&&geo_viewport_integral(f[1],599,&y)&&
  geo_viewport_integral(f[2],800,&w)&&geo_viewport_integral(f[3],600,&h)&&w&&h&&
  x<=800-w&&y<=600-h&&!f[4]&&f[5]==0x3f800000u;
}
#endif
