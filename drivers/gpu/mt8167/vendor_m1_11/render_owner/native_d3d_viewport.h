/* SPDX-License-Identifier: MIT */
#ifndef A04_NATIVE_D3D_VIEWPORT_H
#define A04_NATIVE_D3D_VIEWPORT_H
#include <stdint.h>
/* Limited development profile: integral nonnegative rectangle inside the
 * actual 800x600 target backing, depth0..1. Binary32 inputs/outputs only;
 * the kernel implementation performs no floating-point instructions.
 * PPP wire order: Xoffset,Xscale,Yoffset,Yscale,Zoffset,Zscale.
 * pvr_emit_ppp_state writes a0,m0,a1,m1,a2,m2; setup_viewport assigns
 * a=center/minDepth and m=extent/2/depthRange. */
int32_t a04_m111_build_d3d_viewport(uint32_t out[6],const uint32_t viewport[6]);
#endif
