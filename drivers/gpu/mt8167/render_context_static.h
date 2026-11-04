/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_CONTEXT_STATIC_H
#define A04_RENDER_CONTEXT_STATIC_H
#include "render_execution.h"
#include "pvr_rogue_fwif_client.h"
struct a04_render_context_static_slot {
    uint64_t pt_store_data,pt_store_code,pt_resume_data,pt_resume_code;
    uint64_t sr_store_data,sr_store_code,sr_resume_data,sr_resume_code;
};
struct a04_render_context_static_input {
    uint64_t vdm_state,geom_state;
    struct a04_render_context_static_slot slot[2];
};
/* Fixed GE8300 Mesa static registers. Ownership is structural only: caller
 * must validate code contents, cache attributes and GPU lifetime separately. */
int a04_render_make_context_static(const struct a04_render_context_static_input *input,
    const struct a04_render_owned_range *ranges,size_t count,struct rogue_fwif_geom_registers_caswitch *out);
#endif
