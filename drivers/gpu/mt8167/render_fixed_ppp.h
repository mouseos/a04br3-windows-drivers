/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_FIXED_PPP_H
#define A04_RENDER_FIXED_PPP_H
#include "render_execution.h"
struct a04_render_fixed_ppp_input { uint64_t fragment_pds,uniform_code,uniform_data; };
struct a04_render_fixed_ppp { uint8_t state[100],finish[12]; };
/* Fixed 64x64 single-sample scratchless triangle. Only typed GPU addresses
 * enter the output. Caller must validate shaders, backing and job lifetime. */
int a04_render_make_fixed_ppp(const struct a04_render_fixed_ppp_input *input,
    const struct a04_render_owned_range *ranges,size_t count,struct a04_render_fixed_ppp *out);
#endif
