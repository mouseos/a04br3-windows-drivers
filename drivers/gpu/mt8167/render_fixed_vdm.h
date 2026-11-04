/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_FIXED_VDM_H
#define A04_RENDER_FIXED_VDM_H
#include "render_execution.h"
struct a04_render_fixed_vdm_input {
    uint64_t descriptor_data,descriptor_code,vertex_data,vertex_code,ppp_state,ppp_finish;
};
/* Fixed scratchless probe only. PPP contents, shader provenance and complete
 * command closure remain caller responsibilities. No GPU work is submitted. */
int a04_render_make_fixed_vdm(const struct a04_render_fixed_vdm_input *input,
    const struct a04_render_owned_range *ranges,size_t count,uint8_t out[60]);
#endif
